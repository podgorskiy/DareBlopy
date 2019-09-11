#pragma once
#include <string>
#include "protobuf/example.pb.h"
#include "MemRefFile.h"
#include "common.h"
#include "thread_pool.h"

namespace Records
{
	enum class DataType
	{
		DT_INVALID = 0,
		DT_FLOAT = 1,
		DT_UINT8 = 4,
		DT_STRING = 7,
		DT_INT64 = 9,
	};

	typedef std::vector<size_t> TensorShape;

	class RecordParser
	{
	public:
		struct FixedLenFeature
		{
			FixedLenFeature() = default;

			FixedLenFeature(const TensorShape& shape, DataType dtype): dtype(dtype), shape(shape)
			{
			}

			FixedLenFeature(const TensorShape& shape, DataType dtype, py::object default_value): dtype(dtype), shape(shape), default_value(default_value)
			{
			}

			std::string key;
			TensorShape shape;
			DataType dtype;
			py::object default_value;
		};

		explicit RecordParser(const py::dict& features, bool run_parallel=true, int worker_count=12);

		void ParseSingleExampleInplace(const std::string& serialized, std::vector<py::object>& output, int batch_index);

		std::vector<py::object> ParseExample(const std::vector<std::string>& serialized);

		std::vector<py::object> ParseSingleExample(const std::string& serialized);
	private:
		void ParseSingleExampleImpl(const std::string& serialized, std::vector<void*>& output, int batch_index);

		std::vector<FixedLenFeature> fixed_len_features;
		ThreadPool m_threadPool;
		bool m_run_parallel;
	};
}
