#pragma once
#include "record_readers.h"
#include <vector>
#include <string>
#include <random>
#include <atomic>
#include <mutex>
#include <assert.h>
#include <thread>
#include <condition_variable>


class RecordYielderBasic
{
public:
	explicit RecordYielderBasic(std::vector<std::string>& filenames)
	{
		m_filenames = filenames;
		m_current_file = 0;
		m_rr = nullptr;
	}

	virtual ~RecordYielderBasic() = default;

	py::object GetNext()
	{
		PyBytesObject* bytesObject = nullptr;

		if (m_rr == nullptr)
		{
			if (m_current_file >= m_filenames.size())
			{
				throw py::stop_iteration();
			}

			m_rr = new RecordReader(m_filenames[m_current_file]);
		}

		auto status = m_rr->GetNext(GetBytesAllocator(bytesObject));
		if (!status.ok())
		{
			if (status.state == fsal::Status::kEOF)
			{
				delete m_rr;
				m_rr = nullptr;
				++m_current_file;
				return GetNext();
			}
			else
			{
				throw runtime_error("Error while iterating RecordReader at offset: %zd", m_rr->offset());
			}
		}
		return py::reinterpret_steal<py::object>((PyObject*) bytesObject);
	}

	std::vector<py::object> GetNextN(int n)
	{
		std::vector<py::object> batch;
		batch.reserve(n);
		for (int i = 0; i < n; ++i)
		{
			while (true)
			{
				PyBytesObject* bytesObject = nullptr;

				if (m_rr == nullptr)
				{
					if (m_current_file >= m_filenames.size())
					{
						if(batch.size() > 0)
						{
							return batch;
						}
						else
						{
							throw py::stop_iteration();
						}
					}

					m_rr = new RecordReader(m_filenames[m_current_file]);
				}

				auto status = m_rr->GetNext(GetBytesAllocator(bytesObject));
				if (!status.ok())
				{
					if (status.state == fsal::Status::kEOF)
					{
						delete m_rr;
						m_rr = nullptr;
						++m_current_file;
						continue;
					}
					else
					{
						throw runtime_error("Error while iterating RecordReader at offset: %zd", m_rr->offset());
					}
				}
				py::object value = py::reinterpret_steal<py::object>((PyObject*) bytesObject);
				batch.push_back(std::move(value));
				break;
			}
		}
		return batch;
	}

private:
	std::vector<std::string> m_filenames;
	RecordReader* m_rr;
	int m_current_file;
};


class RecordYielderRandomized
{
public:
	explicit RecordYielderRandomized(std::vector<std::string>& filenames, int buffsize, int seed, int epoch)
	{
		m_filenames = filenames;
		m_buffsize = buffsize;
		uint64_t hash = ((uint64_t)std::hash<int>{}(seed)) ^ ((uint64_t)std::hash<int>{}(epoch) << 1);
		std::mt19937_64 shuffle_rnd(hash);
		std::shuffle(m_filenames.begin(), m_filenames.end(), shuffle_rnd);

		m_current_file = 0;
		m_rr = nullptr;
		m_rnd = std::mt19937_64(std::hash<int>{}(hash) ^ ((uint64_t)std::hash<int>{}(seed) << 1));
	}

	virtual ~RecordYielderRandomized() = default;

	void FillBuffer()
	{
		while (m_buffer.size() < m_buffsize)
		{
			if (m_current_file >= m_filenames.size())
			{
				return;
			}

			if (m_rr == nullptr)
			{
				m_rr = new RecordReader(m_filenames[m_current_file]);
			}

			PyBytesObject* bytesObject = nullptr;

			auto status = m_rr->GetNext(GetBytesAllocator(bytesObject));
			if (!status.ok())
			{
				if (status.state == fsal::Status::kEOF)
				{
					delete m_rr;
					m_rr = nullptr;
					++m_current_file;
					continue;
				}
				else
				{
					throw runtime_error("Error while iterating RecordReader at offset: %zd", m_rr->offset());
				}
			}
			auto index = m_rnd() % (m_buffer.size() + 1);
			if (index == m_buffer.size())
			{
				m_buffer.push_back(py::reinterpret_steal<py::object>((PyObject*) bytesObject));
			}
			else
			{
				m_buffer.push_back(std::move(m_buffer[index]));
				m_buffer[index] = std::move(py::reinterpret_steal<py::object>((PyObject*) bytesObject));
			}
		}
	}

	py::object GetNext()
	{
		FillBuffer();

		if (!m_buffer.empty())
		{
			py::object value = std::move(m_buffer.back());
			m_buffer.pop_back();
			return std::move(value);
		}
		else
		{
			throw py::stop_iteration();
		}
	}

	std::vector<py::object> GetNextN(int n)
	{
		std::vector<py::object> batch;
		batch.reserve(n);
		for (int i = 0; i < n; ++i)
		{
			FillBuffer();

			if (!m_buffer.empty())
			{
				py::object value = std::move(m_buffer.back());
				m_buffer.pop_back();
				batch.push_back(std::move(value));
			}
			else if(batch.size() > 0)
			{
				return batch;
			}
			else
			{
				throw py::stop_iteration();
			}
		}
		return batch;
	}

private:
	std::mt19937_64 m_rnd;
	std::vector<std::string> m_filenames;
	std::vector<py::object> m_buffer;
	int m_buffsize;
	RecordReader* m_rr;
	int m_current_file;
};


class RecordYielderParallel
{
public:
	explicit RecordYielderParallel(std::vector<std::string>& filenames, int buffsize, int parallelism, int seed, int epoch)
	{
		m_parallelism = parallelism;
		m_filenames = filenames;

		m_buffsize = buffsize;
		uint64_t hash = ((uint64_t)std::hash<int>{}(seed)) ^ ((uint64_t)std::hash<int>{}(epoch) << 1);
		std::mt19937_64 shuffle_rnd(hash);
		std::shuffle(m_filenames.begin(), m_filenames.end(), shuffle_rnd);

		m_current_file = 0;
		m_rr = nullptr;
		m_rnd = std::mt19937_64(std::hash<int>{}(hash) ^ ((uint64_t)std::hash<int>{}(seed) << 1));
		m_reached_end = false;
	}

	virtual ~RecordYielderParallel() = default;

	py::object GetNext()
	{
		std::unique_lock<std::mutex> locker(m_queueLock);

		while (m_buffer.size() < m_buffsize / 2 && !m_reached_end)
		{
			m_buffer_enough.wait(locker);
		}

		if (!m_buffer.empty())
		{
			py::object value = std::move(m_buffer.back());
			m_buffer.pop_back();
			return std::move(value);
		}
		else
		{
			throw py::stop_iteration();
		}
	}

private:
	std::mutex m_queueLock;
	std::condition_variable m_buffer_enough;
	std::condition_variable m_emptyQueue;
	bool m_reached_end;

	std::mt19937_64 m_rnd;
	int m_parallelism;
	std::vector<std::string> m_filenames;
	std::vector<py::object> m_buffer;
	int m_buffsize;
	RecordReader* m_rr;
	int m_current_file;
};
