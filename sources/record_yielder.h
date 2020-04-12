//   Copyright 2019-2020 Stanislav Pidhorskyi
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#pragma once
#include "record_readers.h"
#include "example.h"
#include <vector>
#include <string>
#include <random>
#include <atomic>
#include <mutex>
#include <assert.h>
#include <thread>
#include <condition_variable>


class HIDDEN RecordYielderBasic
{
public:
	RecordYielderBasic(const RecordYielderBasic&) = delete; // non construction-copyable
	RecordYielderBasic& operator=( const RecordYielderBasic&) = delete; // non copyable

	explicit RecordYielderBasic(std::vector<std::string>& filenames)
	{
		m_filenames = filenames;
		m_current_file = 0;
		m_rr = nullptr;
	}

	virtual ~RecordYielderBasic()
	{
		delete m_rr;
	}

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
		if (!status.ok() || status.is_eof())
		{
			if (status.is_eof())
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

	py::list GetNextN(int n)
	{
		py::list batch;
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
				if (!status.ok() || status.is_eof())
				{
					if (status.is_eof())
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
				batch.append(std::move(value));
				break;
			}
		}
		return std::move(batch);
	}

private:
	std::vector<std::string> m_filenames;
	RecordReader* m_rr;
	int m_current_file;
};


class HIDDEN RecordYielderRandomized
{
public:
	RecordYielderRandomized(const RecordYielderRandomized&) = delete; // non construction-copyable
	RecordYielderRandomized& operator=( const RecordYielderRandomized&) = delete; // non copyable

	explicit RecordYielderRandomized(std::vector<std::string>& filenames, int buffsize, uint64_t seed, int epoch)
	{
		m_filenames = filenames;
		m_buffsize = buffsize;
		uint64_t hash = ((uint64_t)std::hash<size_t>{}(seed)) ^ ((uint64_t)std::hash<int>{}(epoch) << 1);
		std::mt19937_64 shuffle_rnd(hash);
		std::shuffle(m_filenames.begin(), m_filenames.end(), shuffle_rnd);

		m_current_file = 0;
		m_rr = nullptr;
		m_rnd = std::mt19937_64(std::hash<int>{}(hash) ^ ((uint64_t)std::hash<int>{}(seed) << 1));
	}

	virtual ~RecordYielderRandomized()
	{
		delete m_rr;
	}

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
			if (!status.ok() || status.is_eof())
			{
				if (status.is_eof())
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

	py::list GetNextN(int n)
	{
		py::list  batch;
		for (int i = 0; i < n; ++i)
		{
			FillBuffer();

			if (!m_buffer.empty())
			{
				py::object value = std::move(m_buffer.back());
				m_buffer.pop_back();
				batch.append(std::move(value));
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
		return std::move(batch);
	}

private:
	std::mt19937_64 m_rnd;
	std::vector<std::string> m_filenames;
	std::vector<py::object> m_buffer;
	int m_buffsize;
	RecordReader* m_rr;
	int m_current_file;
};


class HIDDEN ParsedRecordYielderRandomized
{
public:
	ParsedRecordYielderRandomized(const ParsedRecordYielderRandomized&) = delete; // non construction-copyable
	ParsedRecordYielderRandomized& operator=( const ParsedRecordYielderRandomized&) = delete; // non copyable

	explicit ParsedRecordYielderRandomized(py::object parser, std::vector<std::string>& filenames, int buffsize, uint64_t seed, int epoch)
	{
		m_parser_obj = parser;
		m_parser = py::cast<Records::RecordParser*>(m_parser_obj);
		m_filenames = filenames;
		m_buffsize = buffsize;
		uint64_t hash = ((uint64_t)std::hash<size_t>{}(seed)) ^ ((uint64_t)std::hash<int>{}(epoch) << 1);
		std::mt19937_64 shuffle_rnd(hash);
		std::shuffle(m_filenames.begin(), m_filenames.end(), shuffle_rnd);

		m_current_file = 0;
		m_rr = nullptr;
		m_rnd = std::mt19937_64(std::hash<int>{}(hash) ^ ((uint64_t)std::hash<int>{}(seed) << 1));
	}

	virtual ~ParsedRecordYielderRandomized()
	{
		delete m_rr;
	}

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

			std::string str;
			auto alloc = [&str](size_t size)
			{
				str.resize(size + sizeof(uint32_t));
				return &str[0];
			};
			auto status = m_rr->GetNext(alloc);
			if (!status.ok() || status.is_eof())
			{
				if (status.is_eof())
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
				m_buffer.push_back(std::move(str));
			}
			else
			{
				m_buffer.push_back(std::move(m_buffer[index]));
				m_buffer[index] = std::move(str);
			}
		}
	}

	py::object GetNext()
	{
		FillBuffer();

		if (!m_buffer.empty())
		{
			std::string value = std::move(m_buffer.back());
			m_buffer.pop_back();
			return m_parser->ParseSingleExample(value);
		}
		else
		{
			throw py::stop_iteration();
		}
	}

	py::list GetNextN(int n)
	{
		std::vector<std::string>  batch;
		for (int i = 0; i < n; ++i)
		{
			FillBuffer();

			if (!m_buffer.empty())
			{
				std::string value = std::move(m_buffer.back());
				m_buffer.pop_back();
				batch.push_back(std::move(value));
			}
			else if(batch.size() > 0)
			{
				return m_parser->ParseExample(batch);
			}
			else
			{
				throw py::stop_iteration();
			}
		}
		return std::move(m_parser->ParseExample(batch));
	}

private:
	std::mt19937_64 m_rnd;
	std::vector<std::string> m_filenames;
	std::vector<std::string> m_buffer;
	int m_buffsize;
	RecordReader* m_rr;
	int m_current_file;
	py::object m_parser_obj;
	Records::RecordParser* m_parser;
};
