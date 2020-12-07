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

#include "common.h"

#include <pybind11/operators.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <memory>
#include <mutex>
#include <fsal.h>
#include <StdFile.h>
#include <cstdio>
#include <sstream>
#ifdef __linux
#include <sys/stat.h>
#endif

#include "jpeg_decoder.h"
#include "protobuf/example.pb.h"

#include "record_readers.h"
#include "record_yielder.h"
#include "example.h"


int main()
{
	return 0;
}


static fsal::File openfile(const char* filename, fsal::StdFile& tmp_std)
{
	auto fp = std::fopen(filename, "rb");
	if (!fp)
	{
		throw runtime_error("No such file %s", filename);
	}
	tmp_std.AssignFile(fp);
	return fsal::File(&tmp_std, fsal::File::borrow{});
}


static py::object read_as_bytes(const fsal::File& fp)
{
	size_t retSize = 0;
	PyBytesObject* bytesObject = nullptr;
	size_t size = fp.GetSize();
	bytesObject = (PyBytesObject*) PyObject_Malloc(offsetof(PyBytesObject, ob_sval) + size + 1);
	PyObject_INIT_VAR(bytesObject, &PyBytes_Type, size);
	bytesObject->ob_shash = -1;
	bytesObject->ob_sval[size] = '\0';

	fp.Read((uint8_t*)bytesObject->ob_sval, size, &retSize);

	if (retSize != size)
	{
		throw runtime_error("Error reading file. Expected to read %zd bytes, but read only %zd", size, retSize);
	}

	return py::reinterpret_steal<py::object>((PyObject*)bytesObject);
}

void fix_shape(const py::object& _shape, size_t size, std::vector<size_t>& shape_)
{
	shape_.clear();
	if (!_shape.is(py::none()))
	{
		auto shape = py::cast<py::tuple>(_shape);
		bool has_minus = false;
		size_t mul = 1;
		int minus_id = -1;
		for (int i = 0;  i < shape.size(); ++i)
		{
			auto s = py::cast<ptrdiff_t>(shape[i]);
			if (s < 1)
			{
				if (has_minus)
				{
					throw runtime_error("Invalid shape. Can not have more than one unspecified dimention");
				}
				has_minus = true;
				minus_id = i;
			}
			else
			{
				mul *= s;
			}

			shape_.push_back(s);
		}
		if (has_minus)
		{
			if (size % mul == 0)
			{
				shape_[minus_id] = size / mul;
			}
			else
			{
				throw runtime_error("Can't reshape. Total number of elements is %zd and it is not divisible by "
						"specified shape %zd", size, mul);
			}
		}
	}
	else
	{
		shape_.push_back(size);
	}
}


static py::object read_as_numpy_ubyte(const fsal::File& fp, const py::object& _shape)
{
	size_t size = fp.GetSize();

	std::vector<size_t> shape;
	fix_shape(_shape, size, shape);

	ndarray_uint8 data(shape);
	void* ptr = data.request().ptr;
	size_t retSize = -1;
	{
		py::gil_scoped_release release;
		fp.Read((uint8_t*)ptr, size, &retSize);
	}
	if (retSize != size)
	{
		throw runtime_error("Error reading file. Expected to read %zd bytes, but read only %zd", size, retSize);
	}
	return data;
}

static py::object read_jpg_as_numpy(const fsal::File& fp, bool use_turbo)
{
	size_t size = fp.GetSize();
	size_t retSize = 0;
	void* data = nullptr;
	{
		py::gil_scoped_release release;
		data = malloc(size);
		fp.Read((uint8_t*)data, size, &retSize);
	}

	py::object result;

	if (use_turbo)
	{
		result = decode_jpeg_turbo(data, size);
	}
	else
	{
		result = decode_jpeg_vanila(data, size);
	}
	free(data);
	return result;
}

PYBIND11_MODULE(_dareblopy, m)
{
	m.doc() = "_dareblopy - DareBlopy";

	py::enum_<Records::DataType>(m, "DataType", py::arithmetic(), R"(
	    Enumeration for :class:`.FixedLenFeature` dtype.

	    Equivalent to `tf.string`, `tf.float32`, `tf.int64`
	    Note:

	        uint8 - is an alias for string, that enables reading directly to a preallocated numpy ndarray of a uint8 dtype
	        and a given shape. This eliminates any additional copying/casting.
			To use it, shape of the encoded numpy array most be known

	    Example:

	        ::

	            features = {
	                'shape': db.FixedLenFeature([3], db.int64),
	                'data': db.FixedLenFeature([], db.string)
	            }

	)")
			.value("string", Records::DataType::DT_STRING)
			.value("float32", Records::DataType::DT_FLOAT)
			.value("int64", Records::DataType::DT_INT64)
			.value("uint8", Records::DataType::DT_UINT8)
			.export_values();

	py::enum_<RecordReader::Compression>(m, "Compression", py::arithmetic(), R"(
	    Enumeration for compression type used for tfrecords.

	    Possible values:

            * `NONE` - default
            * `GZIP`
            * `ZLIB`

	    Example::

                record_reader = db.RecordReader('zlib_compressed.tfrecords', db.Compression.ZLIB)
                record_reader = db.RecordReader('gzip_compressed.tfrecords', db.Compression.GZIP)

                record_yielder = db.RecordYielderBasic(['test_utils/test-small-gzip-r00.tfrecords',
                                                        'test_utils/test-small-gzip-r01.tfrecords',
                                                        'test_utils/test-small-gzip-r02.tfrecords',
                                                        'test_utils/test-small-gzip-r03.tfrecords'], db.Compression.GZIP)

                record_yielder_random = db.RecordYielderRandomized(['test_utils/test-small-gzip-r00.tfrecords',
                                                                    'test_utils/test-small-gzip-r01.tfrecords',
                                                                    'test_utils/test-small-gzip-r02.tfrecords',
                                                                    'test_utils/test-small-gzip-r03.tfrecords'],
                                                                    buffer_size=16,
                                                                    seed=0,
                                                                    epoch=0,
                                                                    db.Compression.GZIP)
	)")
			.value("NONE", RecordReader::None)
			.value("GZIP", RecordReader::GZIP)
			.value("ZLIB", RecordReader::ZLIB)
			.export_values();

	py::class_<RecordReader>(m, "RecordReader", R"(
	    An iterator that reads tfrecord file and returns raw records (protobuffer messages).
	    Does not support compressed tfrecords. Performs crc32 check of read data.

	    Args:
	    	    file (File): a :ref:`.File` fileobject.
	    	    filename (str): a filename of the file.
	    	    compression (Compression, optional): compression type. Default is Compression.None.

	    Note:
	    	    Contructor is overloaded and excepts either `file` (File) either `filename` (str)

	    Example::

	        rr = db.RecordReader('test_utils/test-small-r00.tfrecords')
	        file_size, data_size, entries = rr.get_metadata()
	        records = list(rr)

	        # Or for the compressed records:
	        rr = db.RecordReader('test_utils/test-small-gzip-r00.tfrecords', db.Compression.GZIP)
	        file_size, data_size, entries = rr.get_metadata()
	        records = list(rr)
	)")
			.def(py::init<fsal::File, RecordReader::Compression>(), py::arg("file"), py::arg("compression") = RecordReader::None)
			.def(py::init<const std::string&, RecordReader::Compression>(), py::arg("filename"), py::arg("compression") = RecordReader::None)
			.def("read_record", [](RecordReader& self, size_t& offset)->py::object
			{
				PyBytesObject* bytesObject = nullptr;
				{
					py::gil_scoped_release release;

					fsal::Status result = self.ReadRecord(offset, GetBytesAllocator(bytesObject));
					if (!result.ok() || result.is_eof())
					{
						PyObject_Free(bytesObject);
						throw runtime_error("Error reading record at offset %zd", offset);
					}
				}

				return py::reinterpret_steal<py::object>((PyObject*)bytesObject);
			}, R"(
			    Reads a record at specific offset. In majority of cases, you won't need this method, instead use
			   `RecordReader` as iterator.
			)")
			.def("__iter__", [](py::object& self)->py::object
			{
				return self;
			})
			.def("__next__", [](RecordReader& self)->py::object
			{
				PyBytesObject* bytesObject = nullptr;
				auto status = self.GetNext(GetBytesAllocator(bytesObject));
				if (!status.ok() || status.is_eof())
				{
					if (status.is_eof())
					{
						throw py::stop_iteration();
					}
					else
					{
						throw runtime_error("Error while iterating RecordReader at offset: %zd", self.offset());
					}
				}
				return py::reinterpret_steal<py::object>((PyObject*)bytesObject);
			})
			.def("get_metadata", [](RecordReader& self)
			{
				auto meta = self.GetMetadata();
				return std::make_tuple(meta.file_size, meta.data_size, meta.entries);
			}, R"(
			    Returns metadata of the tfrecord and checks all crc32 checksums.

			    Note:
			        It has to scan the whole file to

			    Returns:
			        Tuple[int, int, int] - file_size, data_size, entries. Where `file_size` - size of the file,
			        `data_size` - size of the data stored in the tfrecord, `entries` - number of entries.

			)");

	py::class_<Records::RecordParser::FixedLenFeature>(m, "FixedLenFeature", R"(
        An iterator that reads a list of tfrecord files and returns single or a batch of records).
        Does not support compressed tfrecords. Performs crc32 check of read data.

        Attributes:
                shape (TensorShape): a :ref:`.TensorShape` object that defines input data shape.
                dtype (DataType): a :ref:`.DataType` object that defines input data type.
                default_value (object, optional): default value.

        Note:
                Contructor is overloaded and excepts either:

                    *  `shape` (List[int]), `datatype` (DataType)
                    *  `shape` (List[int]), `datatype` (DataType), default_value (object)

        Example::

            features = {
                'shape': db.FixedLenFeature([3], db.int64),
                'data': db.FixedLenFeature([], db.string)
            }

            # or

            features = {
                'shape': db.FixedLenFeature([3], db.int64),
                'data': db.FixedLenFeature([3, 32, 32], db.uint8)
            }

	)")
			.def(py::init())
			.def(py::init<std::vector<size_t>, Records::DataType>())
			.def(py::init<std::vector<size_t>, Records::DataType, py::object>())
			.def_readwrite("shape", &Records::RecordParser::FixedLenFeature::shape)
			.def_readwrite("dtype", &Records::RecordParser::FixedLenFeature::dtype)
			.def_readwrite("default_value", &Records::RecordParser::FixedLenFeature::default_value);

	py::class_<Records::RecordParser>(m, "RecordParser")
			.def(py::init<py::dict>())
			.def(py::init<py::dict, bool>())
			.def(py::init<py::dict, bool, int>())
			.def("parse_single_example_inplace", &Records::RecordParser::ParseSingleExampleInplace)
			.def("parse_single_example", &Records::RecordParser::ParseSingleExample)
			.def("parse_example", &Records::RecordParser::ParseExample);

	py::class_<RecordYielderBasic>(m, "RecordYielderBasic", R"(
	    An iterator that reads a list of tfrecord files and returns single or a batch of records).
	    Does not support compressed tfrecords. Performs crc32 check of read data.

	    Args:
	    	    file (File): a :ref:`.File` fileobject.
	    	    filename (str): a filename of the file.
	    	    compression (Compression, optional): compression type. Default is Compression.None.

	    Note:
	    	    Contructor is overloaded and excepts either `file` (File) either `filename` (str)

	    Example::

	            rr = db.RecordReader('test_utils/test-small-r00.tfrecords')
	            file_size, data_size, entries = rr.get_metadata()
	            records = list(rr)

	            # Or for the compressed records:

	            rr = db.RecordReader('test_utils/test-small-gzip-r00.tfrecords', db.Compression.GZIP)
	            file_size, data_size, entries = rr.get_metadata()
	            records = list(rr)

	)")
			.def(py::init<std::vector<std::string>&, RecordReader::Compression>(), py::arg("filenames"), py::arg("compression") = RecordReader::None)
			.def("__iter__", [](py::object& self)->py::object
			{
				return self;
			})
			.def("__next__", &RecordYielderBasic::GetNext, py::return_value_policy::take_ownership)
	        .def("next_n", &RecordYielderBasic::GetNextN, py::return_value_policy::take_ownership);

	py::class_<RecordYielderRandomized>(m, "RecordYielderRandomized")
			.def(py::init<std::vector<std::string>&, int, uint64_t, int, RecordReader::Compression>(),
			        py::arg("filenames"),  py::arg("buffer_size"),  py::arg("seed"),  py::arg("epoch"), py::arg("compression") = RecordReader::None)
			.def("__iter__", [](py::object& self)->py::object
			{
				return self;
			})
			.def("__next__", &RecordYielderRandomized::GetNext, py::return_value_policy::take_ownership)
			.def("next_n", &RecordYielderRandomized::GetNextN, py::return_value_policy::take_ownership);

	py::class_<ParsedRecordYielderRandomized>(m, "ParsedRecordYielderRandomized")
			.def(py::init<py::object, std::vector<std::string>&, int, uint64_t, int, RecordReader::Compression>(),
			        py::arg("parser"), py::arg("filenames"),  py::arg("buffer_size"),  py::arg("seed"),  py::arg("epoch"), py::arg("compression") = RecordReader::None)
			.def("__iter__", [](py::object& self)->py::object
			{
				return self;
			})
			.def("__next__", &ParsedRecordYielderRandomized::GetNext, py::return_value_policy::take_ownership)
			.def("next_n", &ParsedRecordYielderRandomized::GetNextN, py::return_value_policy::take_ownership);

	m.def("open_as_bytes", [](const char* filename)
	{
		py::gil_scoped_release release;
		fsal::StdFile tmp_std;
		auto fp = openfile(filename, tmp_std);
		return read_as_bytes(fp);
	});

	m.def("open_as_numpy_ubyte", [](const char* filename, py::object shape)
	{
		fsal::StdFile tmp_std;
		fsal::File fp;
		{
			py::gil_scoped_release release;
			fp = openfile(filename, tmp_std);
		}
		return read_as_numpy_ubyte(fp, shape);
	},  py::arg("filename"),  py::arg("shape").none(true) = py::none());

	m.def("read_jpg_as_numpy", [](const char* filename, bool use_turbo)
	{
		fsal::StdFile tmp_std;
		fsal::File fp;
		{
			py::gil_scoped_release release;
			fp = openfile(filename, tmp_std);
		}
		return read_jpg_as_numpy(fp, use_turbo);
	},  py::arg("filename"),  py::arg("use_turbo") = false);

	py::enum_<fsal::Mode>(m, "Mode", py::arithmetic())
		.value("read", fsal::Mode::kRead)
		.value("write", fsal::Mode::kWrite)
		.value("append", fsal::Mode::kAppend)
		.value("read_update", fsal::Mode::kReadUpdate)
		.value("write_update", fsal::Mode::kWriteUpdate)
		.value("append_update", fsal::Mode::kAppendUpdate)
		.export_values();

	py::class_<fsal::Location>(m, "Location")
		.def(py::init<const char*>())
		.def(py::init<const char*, fsal::Location::Options, fsal::PathType, fsal::LinkType>());

	py::implicitly_convertible<const char*, fsal::Location>();

	m.def("open_zip_archive", [](const char* filename)
	{
		fsal::FileSystem fs;
		auto archive_file = fs.Open(filename, fsal::Mode::kRead, true);
		if (!archive_file)
		{
			throw runtime_error("Can't open archive. File: %s not found", filename);
		}
		auto zipreader = new fsal::ZipReader;
		zipreader->OpenArchive(archive_file);
		return new fsal::Archive(fsal::ArchiveReaderInterfacePtr(static_cast<fsal::ArchiveReaderInterface*>(zipreader)));
	});

	m.def("open_zip_archive", [](fsal::File file)
	{
		if (!file)
		{
			throw runtime_error("Can't open archive, argument `file` is None");
		}
		auto* zipreader = new fsal::ZipReader();
		zipreader->OpenArchive(file);
		return new fsal::Archive(fsal::ArchiveReaderInterfacePtr(static_cast<fsal::ArchiveReaderInterface*>(zipreader)));
	});

	py::class_<fsal::Archive> Archive(m, "Archive");
		Archive.def("open", [](fsal::Archive& self, const std::string& filepath)->py::object{
			fsal::File f;
			{
				py::gil_scoped_release release;
				f = self.OpenFile(filepath);
			}
			if (f)
			{
				return py::cast(f);
			}
			else
			{
				return py::cast<py::none>(Py_None);
			}
		}, "Opens file")
		.def("open_as_bytes", [](fsal::Archive& self, const std::string& filepath)->py::object
		{
			PyBytesObject* bytesObject = nullptr;
			{
				py::gil_scoped_release release;

				void* result = self.OpenFile(filepath, GetBytesAllocator(bytesObject));
				if (!result)
				{
					PyObject_Free(bytesObject);
					throw runtime_error("Can't open file: %s", filepath.c_str());
				}

			}

			return py::reinterpret_steal<py::object>((PyObject*)bytesObject);
		})
		.def("open_as_numpy_ubyte", [](fsal::Archive& self, const std::string& filepath, py::object _shape)
		{
			size_t size = 0;
			std::vector<size_t> shape;
			ndarray_uint8 data;
			{
				auto alloc = [&size, &data, &_shape, &shape](size_t s)
				{
					py::gil_scoped_acquire acquire;
					size = s;
					fix_shape(_shape, size, shape);
					data = ndarray_uint8(shape);
					void* ptr = data.request().ptr;
					return ptr;
				};
				{
					py::gil_scoped_release release;
					void* result = self.OpenFile(filepath, alloc);
					if (!result)
					{
						throw runtime_error("Can't open file: %s", filepath.c_str());
					}
				}
			}
			return data;
		})
		.def("read_jpg_as_numpy", [](fsal::Archive& self, const std::string& filepath, bool use_turbo)
		{
			size_t size = 0;
			std::shared_ptr<uint8_t> data;
			{
				py::gil_scoped_release release;
				auto alloc = [&size, &data](size_t s)
				{
					size = s;
					data = std::shared_ptr<uint8_t>((uint8_t*)malloc(size), [](uint8_t*p) {free(p);});
					return data.get();
				};
				void* f = self.OpenFile(filepath, alloc);
				if (!f)
				{
					throw runtime_error("Can't open file: %s", filepath.c_str());
				}
			}
			if (use_turbo)
			{
				return decode_jpeg_turbo(data.get(), size);
			}
			else
			{
				return decode_jpeg_vanila(data.get(), size);
			}
		},  py::arg("filename"),  py::arg("use_turbo") = false)
		.def("exists", [](fsal::Archive& self, const std::string& filepath){
			return self.Exists(filepath);
		}, "Exists")
		.def("list_directory", [](fsal::Archive& self, const std::string& filepath){
			return self.Exists(filepath);
		}, "ListDirectory");

	py::class_<fsal::FileSystem>(m, "FileSystem")
		.def(py::init())
		.def("open", [](fsal::FileSystem& fs, const fsal::Location& location, fsal::Mode mode, bool lockable)->py::object{
				fsal::File f = fs.Open(location, mode, lockable);
				if (f)
				{
					return py::cast(f);
				}
				else
				{
					return py::cast<py::none>(Py_None);
				}
			}, py::arg("location"), py::arg("mode") = fsal::kRead, py::arg("lockable") = false, "Opens file")
		.def("exists", &fsal::FileSystem::Exists, "Exists")
		.def("rename", &fsal::FileSystem::Rename, "Rename")
		.def("remove", &fsal::FileSystem::Remove, "Remove")
		.def("create_directory", &fsal::FileSystem::CreateDirectory, "CreateDirectory")
		.def("push_search_path", &fsal::FileSystem::PushSearchPath, "PushSearchPath")
		.def("pop_search_path", &fsal::FileSystem::PopSearchPath, "PopSearchPath")
		.def("clear_search_paths", &fsal::FileSystem::ClearSearchPaths, "ClearSearchPaths")
		.def("mount_archive", &fsal::FileSystem::MountArchive, "AddArchive");

	py::class_<fsal::File>(m, "File")
			.def(py::init())
			.def("read", [](fsal::File& self, ptrdiff_t size) -> py::object
			{
				size_t position = self.Tell();
				size_t file_size = self.GetSize();
				const char* ptr = (const char*)self.GetDataPointer();
				if (size < 0)
				{
					size = file_size - position;
				}
				if (ptr)
				{
					self.Seek(position + size);
					return py::bytes(ptr + position, size); // py::bytes copies data, because PyBytes_FromStringAndSize copies. No way around
				}
				else
				{
					PyBytesObject* bytesObject = (PyBytesObject *)PyObject_Malloc(offsetof(PyBytesObject, ob_sval) + size + 1);
					PyObject_INIT_VAR(bytesObject, &PyBytes_Type, size);
					bytesObject->ob_shash = -1;
					bytesObject->ob_sval[size] = '\0';
					self.Read((uint8_t*)bytesObject->ob_sval, size);
					return py::reinterpret_steal<py::object>((PyObject*)bytesObject);
				}
			}, py::arg("size") = -1, py::return_value_policy::take_ownership)
			.def("seek", [](fsal::File& self, ptrdiff_t offset, int origin){
				self.Seek(offset, fsal::File::Origin(origin));
				return self.Tell();
			}, py::arg("offset"), py::arg("origin") = 0)
			.def("tell", &fsal::File::Tell)
			.def("size", &fsal::File::GetSize)
			.def("get_last_write_time", &fsal::File::GetLastWriteTime)
			.def("path", [](fsal::File& self){
				return self.GetPath().string();
			});


	py::class_<fsal::Status>(m, "Status")
			.def(py::init())
			.def("__nonzero__", &fsal::Status::ok)
			.def("is_eof", &fsal::Status::is_eof);
}
