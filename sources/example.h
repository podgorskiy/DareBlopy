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

	const char* DataTypeString(DataType dtype);

	typedef std::vector<size_t> TensorShape;

	inline size_t num_elements(TensorShape shape)
	{
		size_t num = 1;
		for (auto s: shape)
		{
			num *= s;
		}
		return num;
	}

	inline DataType Feature2DataType(Feature feature)
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

	inline std::string Shape2str(const TensorShape& shape)
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

	inline bool FeatureDecode(const std::size_t out_index, const std::string& key, const DataType& dtype,
	                   const TensorShape& shape, const Feature& feature, py::object& tensor)
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
				void* ptr;
				{
					py::gil_scoped_acquire acquire;
					ndarray_int64 out = py::cast<ndarray_int64>(tensor);
					ptr = out.request().ptr;
				}
				auto out_p = (int64_t*)ptr + offset;
				std::copy_n(values.value().data(), num, out_p);
				return true;
			}
			case DataType::DT_FLOAT:
			{
				const FloatList& values = feature.float_list();
				if (static_cast<size_t>(values.value_size()) != num)
				{
					throw runtime_error("Key: %s. Number of float values != expected. Values size: %zd but output shape: %s", key.c_str(), values.value_size(), Shape2str(shape).c_str());
				}
				void* ptr;
				{
					py::gil_scoped_acquire acquire;
					ndarray_float32 out = py::cast<ndarray_float32>(tensor);
					ptr = out.request().ptr;
				}
				auto out_p = (float*)ptr + offset;
				std::copy_n(values.value().data(), num, out_p);
				return true;
			}
			case DataType::DT_STRING:
			{
				const BytesList& values = feature.bytes_list();
				if (static_cast<size_t>(values.value_size()) != num)
				{
					throw runtime_error("Key: %s. Number of bytes values != expected. Values size: %zd but output shape: %s", key.c_str(), values.value_size(), Shape2str(shape).c_str());
				}
				ndarray_object out = py::cast<ndarray_object>(tensor);
				auto ptr = out.mutable_unchecked();
				for (int i = 0; i < num; ++i)
				{
					const std::string& s = *values.value().data()[i];
					ptr(i + offset) = py::bytes(s);
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
				uint8_t* ptr;
				{
					py::gil_scoped_acquire acquire;
					ndarray_uint8 out = py::cast<ndarray_uint8>(tensor);
					ptr = (uint8_t*)out.request().ptr;
				}
				ptr += offset;
				for (int i = 0; i < values.value_size(); ++i)
				{
					const std::string& s = *values.value().data()[i];
					std::copy_n(s.data(), s.size(), ptr);
					ptr += s.size();
				}
				return true;
			}
			default:
				throw runtime_error("Invalid input dtype: %s", DataTypeString(dtype));
		}
	}

	inline py::object TensorFactory(DataType dtype, const TensorShape& shape)
	{
		switch (dtype)
		{
			case DataType::DT_INT64:
			{
				return ndarray_int64(shape);
			}
			case DataType::DT_FLOAT:
			{
				return ndarray_float32(shape);
			}
			case DataType::DT_UINT8:
			{
				return ndarray_uint8(shape);
			}
			case DataType::DT_STRING:
			{
				if (shape.size() == 0)
				{
					return ndarray_object({1});
				}

				return ndarray_object(shape);
			}
			case DataType::DT_INVALID:
			default:
			{
				throw runtime_error("Invalid input dtype: %s", DataTypeString(dtype));
			}
		}
	}

	inline bool FeatureDecodePtr(const std::size_t out_index, const std::string& key, const DataType& dtype,
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

	inline std::pair<py::object, void*> TensorFactoryPtr(DataType dtype, const TensorShape& shape)
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

		explicit RecordParser(const py::dict& features, bool run_parallel=true): m_run_parallel(run_parallel), m_threadPool(12)
		{
			for (auto item : features)
			{
				const std::string& key = py::cast<std::string>(item.first);
				auto fixedLenFeature = py::cast<FixedLenFeature>(item.second);
				fixedLenFeature.key = key;
				fixed_len_features.push_back(fixedLenFeature);
			}
		}

		void ParseSingleExampleImpl(const std::string& serialized, std::vector<py::object>& output, int batch_index)
		{
			Example example;
			//example.ParseFromArray(file->GetDataPointer(), file->GetSize());
			example.ParseFromString(serialized);

			const Features& features = example.features();
			const auto& feature_dict = features.feature();

			for (size_t d = 0; d < fixed_len_features.size(); ++d)
			{
				const FixedLenFeature& feature_config = fixed_len_features[d];
				const std::string& key = feature_config.key;
				const DataType& dtype = feature_config.dtype;
				const TensorShape& shape = feature_config.shape;
				const py::object& default_value = feature_config.default_value;
				bool required = !default_value;

				const auto& feature_found = feature_dict.find(key);
				const bool feature_has_data = (feature_found != feature_dict.end() &&
				                               (feature_found->second.kind_case() != Feature::KIND_NOT_SET));

				const bool required_ok = feature_has_data || !required;
				if (!required_ok)
				{
					throw runtime_error("Feature %s is required but could not be found.", key.c_str());
				}

				if (feature_has_data)
				{
					const Feature& f = feature_found->second;
					DataType tmp_dtype = dtype;
					if (tmp_dtype == DataType::DT_UINT8)
					{
						tmp_dtype = DataType::DT_STRING;
					}

					if (Feature2DataType(f) != tmp_dtype)
					{
						throw runtime_error(
								//"Name: %s, "
								"Feature: %s. Data types don't match. Expected type: %s,  Feature is: %s",
								key.c_str(), DataTypeString(dtype), f.DebugString().c_str());

					}
					FeatureDecode(batch_index, key, dtype, shape, f, output[d]);
				} else {
					// If the value is missing, RowDenseCopy the default value.
//					RowDenseCopy(batch_index, dtype, default_value,
//					             (*output_dense_values_tensor)[d]);
				}
			}
		}

		std::vector<py::object> ParseExample(const std::vector<std::string>& serialized)
		{
			std::vector<py::object> tensors;
			tensors.reserve(fixed_len_features.size());

			for (const auto& feature_config: fixed_len_features)
			{
				TensorShape out_shape(feature_config.shape.size() + 1);
				memcpy(&out_shape[1], feature_config.shape.data(), feature_config.shape.size() * sizeof(size_t));
				out_shape[0] = serialized.size();
				tensors.push_back(TensorFactory(feature_config.dtype, out_shape));
			}

			for (int i = 0, l = serialized.size(); i < l; ++i)
			{
				ParseSingleExampleImpl(serialized[i], tensors, i);
			}
			return tensors;
		}

		std::vector<py::object> ParseSingleExample(const std::string& serialized)
		{
			std::vector<py::object> tensors;
			tensors.reserve(fixed_len_features.size());

			for (const auto& feature_config: fixed_len_features)
			{
				tensors.push_back(TensorFactory(feature_config.dtype, feature_config.shape));
			}

			ParseSingleExampleImpl(serialized, tensors, 0);
			return tensors;
		}

		void ParseSingleExampleImplPtr(const std::string& serialized, std::vector<void*>& output, int batch_index)
		{
			Example example;
			example.ParseFromString(serialized);

			const Features& features = example.features();
			const auto& feature_dict = features.feature();

			for (size_t d = 0; d < fixed_len_features.size(); ++d)
			{
				const FixedLenFeature& feature_config = fixed_len_features[d];
				const std::string& key = feature_config.key;
				const DataType& dtype = feature_config.dtype;
				const TensorShape& shape = feature_config.shape;
				const py::object& default_value = feature_config.default_value;
				bool required = !default_value;

				const auto& feature_found = feature_dict.find(key);
				const bool feature_has_data = (feature_found != feature_dict.end() &&
				                               (feature_found->second.kind_case() != Feature::KIND_NOT_SET));

				const bool required_ok = feature_has_data || !required;
				if (!required_ok)
				{
					throw runtime_error("Feature %s is required but could not be found.", key.c_str());
				}

				if (feature_has_data)
				{
					const Feature& f = feature_found->second;
					DataType tmp_dtype = dtype;
					if (tmp_dtype == DataType::DT_UINT8)
					{
						tmp_dtype = DataType::DT_STRING;
					}

					if (Feature2DataType(f) != tmp_dtype)
					{
						throw runtime_error(
								//"Name: %s, "
						        "Feature: %s. Data types don't match. Expected type: %s,  Feature is: %s",
						        key.c_str(), DataTypeString(dtype), f.DebugString().c_str());

					}
					FeatureDecodePtr(batch_index, key, dtype, shape, f, output[d]);
				} else {
					// If the value is missing, RowDenseCopy the default value.
//					RowDenseCopy(batch_index, dtype, default_value,
//					             (*output_dense_values_tensor)[d]);
				}
			}
		}

		std::vector<py::object> ParseExamplePtr(const std::vector<std::string>& serialized)
		{
			py::gil_scoped_release release;
			std::vector<py::object> tensors;
			std::vector<void*> tensor_ptrs;
			tensors.reserve(fixed_len_features.size());
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
					tensors.push_back(tensor);
					tensor_ptrs.push_back(tensor_ptr);
				}
			}

			if (m_run_parallel)
			{
				ThreadPool::Kernel k =[this, &serialized, &tensor_ptrs](ThreadPool::threadIdx idx, ThreadPool::blockDim)
				{
					ParseSingleExampleImplPtr(serialized[idx], tensor_ptrs, idx);
				};
				m_threadPool.ParallelFor(&k, serialized.size());
			}
			else
			{
				for (int idx = 0, l = serialized.size(); idx < l; ++idx)
				{
					ParseSingleExampleImplPtr(serialized[idx], tensor_ptrs, idx);
				}
			}
			return tensors;
		}

		std::vector<py::object> ParseSingleExamplePtr(const std::string& serialized)
		{
			py::gil_scoped_release release;
			std::vector<py::object> tensors;
			std::vector<void*> tensor_ptrs;
			tensors.reserve(fixed_len_features.size());
			tensor_ptrs.reserve(fixed_len_features.size());

			{
				py::gil_scoped_acquire acquire;
				for (const auto& feature_config: fixed_len_features)
				{
					auto result = TensorFactoryPtr(feature_config.dtype, feature_config.shape);
					auto tensor = result.first;
					auto tensor_ptr = result.second;
					tensors.push_back(tensor);
					tensor_ptrs.push_back(tensor_ptr);
				}
			}

			ParseSingleExampleImplPtr(serialized, tensor_ptrs, 0);
			return tensors;
		}
	private:
		std::vector<FixedLenFeature> fixed_len_features;
		ThreadPool m_threadPool;
		bool m_run_parallel;
	};
}
