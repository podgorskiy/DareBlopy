#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <stdarg.h>
#include <memory>
#include <exception>

#define HIDDEN __attribute__ ((visibility("hidden")))

#define PYBIND11_NUMPY_OBJECT_DTYPE(Type) \
    namespace pybind11 { namespace detail { \
        template <> struct npy_format_descriptor<Type> { \
        public: \
            enum { value = npy_api::NPY_OBJECT_ }; \
            static pybind11::dtype dtype() { \
                if (auto ptr = npy_api::get().PyArray_DescrFromType_(value)) { \
                    return reinterpret_borrow<pybind11::dtype>(ptr); \
                } \
                pybind11_fail("Unsupported buffer format!"); \
            } \
            static constexpr auto name = _("object"); \
        }; \
    }}

namespace py = pybind11;

PYBIND11_NUMPY_OBJECT_DTYPE(py::bytes)
PYBIND11_NUMPY_OBJECT_DTYPE(py::object)


typedef py::array_t<uint8_t, py::array::c_style> ndarray_uint8;
typedef py::array_t<int64_t, py::array::c_style> ndarray_int64;
typedef py::array_t<float, py::array::c_style> ndarray_float32;
typedef py::array_t<py::object, py::array::c_style> ndarray_object;


inline std::string string_format(const std::string fmt_str, va_list ap)
{
	int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
	std::unique_ptr<char[]> formatted;
	while(1) {
		va_list ap_copy;
		va_copy(ap_copy, ap);
		formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
		strcpy(&formatted[0], fmt_str.c_str());
		final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap_copy);
		if (final_n < 0 || final_n >= n)
			n += abs(final_n - n + 1);
		else
			break;
	}
	return std::string(formatted.get());
}

inline std::string string_format(const std::string fmt_str, ...)
{
	va_list ap;
	va_start(ap, fmt_str);
	std::string result = string_format(fmt_str, ap);
	va_end(ap);
	return result;
}

class runtime_error: public std::runtime_error
{
public:
	runtime_error(const std::string fmt_str, ...):
			std::runtime_error(string_format(fmt_str, (va_start(ap, fmt_str), ap)))
	{
		va_end(ap);
	}

	runtime_error(const runtime_error &) = default;
	runtime_error(runtime_error &&) = default;
	~runtime_error() = default;

private:
	va_list ap;
};


inline std::function<void*(size_t)> GetBytesAllocator(PyBytesObject*& bytesObject)
{
	auto alloc = [&bytesObject](size_t size)
	{
		bytesObject = (PyBytesObject*) PyObject_Malloc(offsetof(PyBytesObject, ob_sval) + size + 1);
		size -= sizeof(uint32_t);
		PyObject_INIT_VAR(bytesObject, &PyBytes_Type, size);
		bytesObject->ob_shash = -1;
		bytesObject->ob_sval[size] = '\0';
		return bytesObject->ob_sval;
	};
	return alloc;
}

