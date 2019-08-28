/*
 * Copyright 2019 Stanislav Pidhorskyi. All rights reserved.
 * License: https://raw.githubusercontent.com/podgorskiy/bimpy/master/LICENSE.txt
 */

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <memory>
#include <mutex>
#include <fsal.h>
#include <StdFile.h>
#include <cstdio>
#include <sstream>
#ifdef __linux
#include <sys/stat.h>
#endif
#include <jpeglib.h>
#include <setjmp.h>

namespace py = pybind11;

typedef py::array_t<uint8_t, py::array::c_style> ndarray_uint8;
typedef py::array_t<float, py::array::c_style> ndarray_float32;

int main()
{
	return 0;
}


static fsal::File openfile(const char* filename, fsal::StdFile& tmp_std)
{
	auto fp = std::fopen(filename, "rb");
	if (!fp)
	{
		std::stringstream ss;
		ss << "No such file " << filename;
		PyErr_SetString(PyExc_IOError, ss.str().c_str());
		throw py::error_already_set();
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
		PyErr_SetString(PyExc_IOError, "Error reading file ");
		throw py::error_already_set();
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
					std::stringstream ss;
					ss << "Invalid shape ";
					PyErr_SetString(PyExc_IOError, ss.str().c_str());
					throw py::error_already_set();
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
				std::stringstream ss;
				ss << "Can't reshape";
				PyErr_SetString(PyExc_IOError, ss.str().c_str());
				throw py::error_already_set();
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
		PyErr_SetString(PyExc_IOError, "Error reading file ");
		throw py::error_already_set();
	}
	return data;
}

struct my_error_mgr {
		struct jpeg_error_mgr pub;
		jmp_buf setjmp_buffer;
		void (*emit_message) (j_common_ptr, int);
		boolean warning, stopOnWarning;
};

static py::object read_jpg_as_numpy(const fsal::File& fp)
{
	size_t size = fp.GetSize();
	size_t retSize = 0;
	void* data = nullptr;
	{
		py::gil_scoped_release release;
		data = malloc(size);
		fp.Read((uint8_t*)data, size, &retSize);
	}

	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */
	cinfo.err = jpeg_std_error(&jerr.pub);

	if (setjmp(jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_decompress(&cinfo);
		PyErr_SetString(PyExc_IOError, "Error reading file ");
		throw py::error_already_set();
	}
	/* Now we can initialize the JPEG decompression object. */
	jpeg_create_decompress(&cinfo);
	/* Step 2: specify data source (eg, a file) */
	jpeg_mem_src(&cinfo, (const unsigned char*)data, size);
	/* Step 3: read file parameters with jpeg_read_header() */
	(void) jpeg_read_header(&cinfo, TRUE);
	/* Step 5: Start decompressor */
	(void) jpeg_start_decompress(&cinfo);

	/* We may need to do some setup of our own at this point before reading
	 * the data.  After jpeg_start_decompress() we have the correct scaled
	 * output image dimensions available, as well as the output colormap
	 * if we asked for color quantization.
	 * In this example, we need to make an output work buffer of the right size.
	 */
	/* JSAMPLEs per row in output buffer */
	row_stride = cinfo.output_width * cinfo.output_components;
	/* Make a one-row-high sample array that will go away when done with image */

	std::array<size_t, 3> shape = {cinfo.output_height, cinfo.output_width, 3};
	ndarray_uint8 ar(shape);
	unsigned char* ptr = (unsigned char*)ar.request().ptr;
	//buffer = (*cinfo.mem->alloc_sarray)
	//		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	int i = 0;
	while (cinfo.output_scanline < cinfo.output_height) {
		/* jpeg_read_scanlines expects an array of pointers to scanlines.
		 * Here the array is only one element long, but you could ask for
		 * more than one scanline at a time if that's more convenient.
		 */
		unsigned char* p = (ptr + row_stride * i);
		(void) jpeg_read_scanlines(&cinfo, &p, 1);
		++i;
		/* Assume put_scanline_someplace wants a pointer and sample count. */
		//put_scanline_someplace(buffer[0], row_stride);
	}
	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return ar;
}


PYBIND11_MODULE(_vfsdl, m)
{
	m.doc() = "vfsdl - VirtualFSDataLoader";

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

	m.def("read_jpg_as_numpy", [](const char* filename)
	{
		fsal::StdFile tmp_std;
		fsal::File fp;
		{
			py::gil_scoped_release release;
			fp = openfile(filename, tmp_std);
		}
		return read_jpg_as_numpy(fp);
	},  py::arg("filename"));

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
		auto archive_file = fs.Open(filename);
		auto zr = new fsal::ZipReader;
		zr->OpenArchive(archive_file);
		return zr;
	});

	py::class_<fsal::ArchiveReaderInterface> Archive(m, "Archive");
		Archive.def("open", [](fsal::ArchiveReaderInterface& self, const std::string& filepath)->py::object{
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
		.def("open_as_bytes", [](fsal::ArchiveReaderInterface& self, const std::string& filepath)->py::object
		{
			PyBytesObject* bytesObject = nullptr;
			size_t size = 0;
			{
				py::gil_scoped_release release;

				auto alloc = [&size, &bytesObject](size_t s)
				{
					size = s;
					bytesObject = (PyBytesObject*) PyObject_Malloc(offsetof(PyBytesObject, ob_sval) + size + 1);
					PyObject_INIT_VAR(bytesObject, &PyBytes_Type, size);
					bytesObject->ob_shash = -1;
					bytesObject->ob_sval[size] = '\0';
					return bytesObject->ob_sval;
				};

				void* result = self.OpenFile(filepath, alloc);
				if (!result)
				{
					PyObject_Free(bytesObject);
					PyErr_SetString(PyExc_IOError, "Error reading file ");
					throw py::error_already_set();
				}

			}

			return py::reinterpret_steal<py::object>((PyObject*)bytesObject);
		})
		.def("open_as_numpy_ubyte", [](fsal::ArchiveReaderInterface& self, const std::string& filepath, py::object _shape)
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
						PyErr_SetString(PyExc_IOError, "Error reading file ");
						throw py::error_already_set();
					}
				}
			}
			return data;
		})
		.def("exists", [](fsal::ArchiveReaderInterface& self, const std::string& filepath){
			return self.Exists(filepath);
		}, "Exists")
		.def("list_directory", [](fsal::ArchiveReaderInterface& self, const std::string& filepath){
			return self.Exists(filepath);
		}, "ListDirectory");

	py::class_<fsal::ZipReader>(m, "ZipReader", Archive)
		.def(py::init());

	py::class_<fsal::FileSystem>(m, "FileSystem")
		.def(py::init())
		.def("open", [](fsal::FileSystem& fs, const fsal::Location& location, fsal::Mode mode)->py::object{
				fsal::File f = fs.Open(location, mode);
				if (f)
				{
					return py::cast(f);
				}
				else
				{
					return py::cast<py::none>(Py_None);
				}
			}, py::arg("location"), py::arg("mode") = fsal::kRead, "Opens file")
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
			.def("tell", &fsal::File::Tell);

	py::class_<fsal::Status>(m, "Status")
			.def(py::init())
			.def("__nonzero__", &fsal::Status::ok);
}
