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


#include "example.h"


namespace Records
{
	const char* DataTypeString(DataType dtype);

	std::pair<py::object, void*> TensorFactoryPtr(DataType dtype, const TensorShape& shape);

	void* GetPtr(py::object& tensor, DataType dtype);

	size_t num_elements(const TensorShape& shape);

	DataType Feature2DataType(const Feature& feature);

	std::string Shape2str(const TensorShape& shape);

	bool FeatureDecode(std::size_t out_index, const std::string& key, const DataType& dtype,
	                   const TensorShape& shape, const Feature& feature, void* out_ptr);
}

inline const char* Records::DataTypeString(DataType dtype)
{
	switch (dtype)
	{
		case DataType::DT_FLOAT:
			return "float32";
		case DataType::DT_INT64:
			return "int64";
		case DataType::DT_UINT8:
			return "uint8";
		case DataType::DT_STRING:
			return "string";
		case DataType::DT_INVALID:
		default:
			return "invalid";
	}
}

inline size_t Records::num_elements(const TensorShape& shape)
{
	size_t num = 1;
	for (auto s: shape)
	{
		num *= s;
	}
	return num;
}

inline Records::DataType Records::Feature2DataType(const Feature& feature)
{
	auto feature_type = feature.kind_case();
	switch (feature_type)
	{
		case Feature::kInt64List:
			return DataType::DT_INT64;
		case Feature::kFloatList:
			return DataType::DT_FLOAT;
		case Feature::kBytesList:
			return DataType::DT_STRING;
		default:
			return DataType::DT_INVALID;
	}
}

inline std::string Records::Shape2str(const TensorShape& shape)
{
	std::string shape_str = "[";
	size_t size = shape_str.size();
	for (auto s: shape)
	{
		shape_str += string_format("%zd, ", s);
		size = shape_str.size() - 2;
	}
	shape_str = std::string(shape_str.data(), size);
	shape_str += ']';
	return shape_str;
}


inline std::pair<py::object, void*> Records::TensorFactoryPtr(DataType dtype, const TensorShape& shape)
{
	switch (dtype)
	{
		case DataType::DT_INT64:
		{
			auto tensor = ndarray_int64(shape);
			auto buffer = tensor.request();
			return std::make_pair(tensor, buffer.ptr);
		}
		case DataType::DT_FLOAT:
		{
			auto tensor = ndarray_float32(shape);
			auto buffer = tensor.request();
			return std::make_pair(tensor, buffer.ptr);
		}
		case DataType::DT_UINT8:
		{
			auto tensor = ndarray_uint8(shape);
			auto buffer = tensor.request();
			return std::make_pair(tensor, buffer.ptr);
		}
		case DataType::DT_STRING:
		{
			TensorShape shape_ = shape;
			if (shape.size() == 0)
			{
				shape_ = TensorShape({1});
			}

			auto tensor = ndarray_object(shape_);
			auto buffer = tensor.request();
			return std::make_pair(tensor, buffer.ptr);
		}
		case DataType::DT_INVALID:
		default:
		{
			throw runtime_error("Invalid input dtype: %s", DataTypeString(dtype));
		}
	}
}

inline void* Records::GetPtr(py::object& tensor, DataType dtype)
{
	switch (dtype)
	{
		case DataType::DT_INT64:
		{
			auto buffer = ndarray_int64(tensor).request();
			return buffer.ptr;
		}
		case DataType::DT_FLOAT:
		{
			auto buffer = ndarray_float32(tensor).request();
			return buffer.ptr;
		}
		case DataType::DT_UINT8:
		{
			auto buffer = ndarray_uint8(tensor).request();
			return buffer.ptr;
		}
		case DataType::DT_STRING:
		{
			auto buffer = ndarray_object(tensor).request();
			return buffer.ptr;
		}
		case DataType::DT_INVALID:
		default:
		{
			throw runtime_error("Invalid input dtype: %s", DataTypeString(dtype));
		}
	}
}

bool Records::FeatureDecode(std::size_t out_index, const std::string& key, const DataType& dtype,
                      const TensorShape& shape, const Feature& feature, void* out_ptr)
{
	const std::size_t num = num_elements(shape);
	const std::size_t offset = out_index * num;

	switch (dtype)
	{
		case DataType::DT_INT64:
		{
			const Int64List& values = feature.int64_list();
			if (static_cast<size_t>(values.value_size()) != num)
			{
				throw runtime_error("Key: %s. Number of int64 values != expected. Values size: %zd but output shape: %s", key.c_str(), values.value_size(), Shape2str(shape).c_str());
			}
			auto out_p = (int64_t*)out_ptr + offset;
			memcpy(out_p, values.value().data(), num * sizeof(int64_t));
			return true;
		}
		case DataType::DT_FLOAT:
		{
			const FloatList& values = feature.float_list();
			if (static_cast<size_t>(values.value_size()) != num)
			{
				throw runtime_error("Key: %s. Number of float values != expected. Values size: %zd but output shape: %s", key.c_str(), values.value_size(), Shape2str(shape).c_str());
			}
			auto out_p = (float*)out_ptr + offset;
			memcpy(out_p, values.value().data(), num * sizeof(float));
			return true;
		}
		case DataType::DT_STRING:
		{
			const BytesList& values = feature.bytes_list();
			if (static_cast<size_t>(values.value_size()) != num)
			{
				throw runtime_error("Key: %s. Number of bytes values != expected. Values size: %zd but output shape: %s", key.c_str(), values.value_size(), Shape2str(shape).c_str());
			}
			py::object* ptr = (py::object*)out_ptr + offset;

			for (int i = 0; i < num; ++i)
			{
				const std::string& s = *values.value().data()[i];
				*(ptr + i) = py::bytes(s);
			}

			return true;
		}
		case DataType::DT_UINT8:
		{
			const BytesList& values = feature.bytes_list();
			size_t size = 0;
			for (int i = 0; i < values.value_size(); ++i)
			{
				const std::string& s = *values.value().data()[i];
				size += s.size();
			}
			if (size != num)
			{
				throw runtime_error("Key: %s. Number of uint8 values != expected. Values size: %zd but output shape: %s", key.c_str(), size, Shape2str(shape).c_str());
			}
			uint8_t* ptr = (uint8_t*)out_ptr;
			ptr += offset;
			for (int i = 0; i < values.value_size(); ++i)
			{
				const std::string& s = *values.value().data()[i];
				memcpy(ptr, s.data(), s.size());
				ptr += s.size();
			}
			return true;
		}

		default:
			throw runtime_error("Invalid input dtype: %s", DataTypeString(dtype));
	}
}

Records::RecordParser::RecordParser(const py::dict& features, bool run_parallel, int worker_count): m_run_parallel(run_parallel), m_threadPool(worker_count)
{
	for (auto item : features)
	{
		const std::string& key = py::cast<std::string>(item.first);
		auto fixedLenFeature = py::cast<FixedLenFeature>(item.second);
		fixedLenFeature.key = key;
		fixed_len_features.push_back(fixedLenFeature);
	}
}

void Records::RecordParser::ParseSingleExampleInplace(const std::string& serialized, std::vector<py::object>& output, int batch_index)
{
	Example example;
	example.ParseFromString(serialized);

	const Features& features = example.features();
	const auto& feature_dict = features.feature();

	for (size_t d = 0; d < fixed_len_features.size(); ++d)
	{
		const FixedLenFeature& feature_config = fixed_len_features[d];
		const py::object& default_value = feature_config.default_value;
		bool required = !default_value;

		const auto& feature_found = feature_dict.find(feature_config.key);
		const bool feature_has_data = (feature_found != feature_dict.end() &&
		                               (feature_found->second.kind_case() != Feature::KIND_NOT_SET));

		const bool required_ok = feature_has_data || !required;
		if (!required_ok)
		{
			throw runtime_error("Feature %s is required but could not be found.", feature_config.key.c_str());
		}

		if (feature_has_data)
		{
			const Feature& f = feature_found->second;
			DataType tmp_dtype = feature_config.dtype;
			if (tmp_dtype == DataType::DT_UINT8)
			{
				tmp_dtype = DataType::DT_STRING;
			}

			if (Feature2DataType(f) != tmp_dtype)
			{
				throw runtime_error(
						//"Name: %s, "
						"Feature: %s. Data types don't match. Expected type: %s,  Feature is: %s",
						feature_config.key.c_str(), DataTypeString(feature_config.dtype), f.DebugString().c_str());

			}

			void* output_ = nullptr;
			{
				py::gil_scoped_acquire acquire;
				output_ = GetPtr(output[d], feature_config.dtype);
			}
			FeatureDecode(batch_index, feature_config.key, feature_config.dtype, feature_config.shape, f, output_);
		}
		else
		{
			// TODO, If the value is missing, copy the default
			throw runtime_error("Feature %s data is missing. Default value is not implemented yet", feature_config.key.c_str());
		}
	}
}

void Records::RecordParser::ParseSingleExampleImpl(const std::string& serialized, std::vector<void*>& output, int batch_index)
{
	Example example;
	example.ParseFromString(serialized);

	const Features& features = example.features();
	const auto& feature_dict = features.feature();

	for (size_t d = 0; d < fixed_len_features.size(); ++d)
	{
		const FixedLenFeature& feature_config = fixed_len_features[d];
		const py::object& default_value = feature_config.default_value;
		bool required = !default_value;

		const auto& feature_found = feature_dict.find(feature_config.key);
		const bool feature_has_data = (feature_found != feature_dict.end() &&
		                               (feature_found->second.kind_case() != Feature::KIND_NOT_SET));

		const bool required_ok = feature_has_data || !required;
		if (!required_ok)
		{
			throw runtime_error("Feature %s is required but could not be found.", feature_config.key.c_str());
		}

		if (feature_has_data)
		{
			const Feature& f = feature_found->second;
			DataType tmp_dtype = feature_config.dtype;
			if (tmp_dtype == DataType::DT_UINT8)
			{
				tmp_dtype = DataType::DT_STRING;
			}

			if (Feature2DataType(f) != tmp_dtype)
			{
				throw runtime_error(
						//"Name: %s, "
						"Feature: %s. Data types don't match. Expected type: %s,  Feature is: %s",
						feature_config.key.c_str(), DataTypeString(feature_config.dtype), f.DebugString().c_str());

			}
			FeatureDecode(batch_index, feature_config.key, feature_config.dtype, feature_config.shape, f, output[d]);
		}
		else
		{
			// TODO, If the value is missing, copy the default
			throw runtime_error("Feature %s data is missing. Default value is not implemented yet", feature_config.key.c_str());
		}
	}
}

py::list Records::RecordParser::ParseExample(const std::vector<std::string>& serialized)
{
	py::list tensors;
	std::vector<void*> tensor_ptrs;
	{
		py::gil_scoped_release release;
		tensor_ptrs.reserve(fixed_len_features.size());
		std::vector<std::pair<DataType, TensorShape> > tensorTypeAndShape;
		for (const auto& feature_config: fixed_len_features)
		{
			TensorShape out_shape(feature_config.shape.size() + 1);
			memcpy(&out_shape[1], feature_config.shape.data(), feature_config.shape.size() * sizeof(size_t));
			out_shape[0] = serialized.size();
			tensorTypeAndShape.push_back(std::make_pair(feature_config.dtype, out_shape));
		}
		{
			py::gil_scoped_acquire acquire;
			for (const auto& prop: tensorTypeAndShape)
			{
				auto result = TensorFactoryPtr(prop.first, prop.second);
				auto tensor = result.first;
				auto tensor_ptr = result.second;
				tensors.append(tensor);
				tensor_ptrs.push_back(tensor_ptr);
			}
		}

		if (m_run_parallel)
		{
			ThreadPool::Kernel k =[this, &serialized, &tensor_ptrs](ThreadPool::threadIdx idx, ThreadPool::blockDim)
			{
				ParseSingleExampleImpl(serialized[idx], tensor_ptrs, idx);
			};
			m_threadPool.ParallelFor(&k, serialized.size());
		}
		else
		{
			for (int idx = 0, l = serialized.size(); idx < l; ++idx)
			{
				ParseSingleExampleImpl(serialized[idx], tensor_ptrs, idx);
			}
		}
	}
	return tensors;
}

py::list Records::RecordParser::ParseSingleExample(const std::string& serialized)
{
	py::list  tensors;
	std::vector<void*> tensor_ptrs;
	tensor_ptrs.reserve(fixed_len_features.size());

	for (const auto& feature_config: fixed_len_features)
	{
		auto result = TensorFactoryPtr(feature_config.dtype, feature_config.shape);
		auto tensor = result.first;
		auto tensor_ptr = result.second;
		tensors.append(tensor);
		tensor_ptrs.push_back(tensor_ptr);
	}

	py::gil_scoped_release release;
	ParseSingleExampleImpl(serialized, tensor_ptrs, 0);
	return tensors;
}
