// Code below is mostly copy-paste of example.c from libjpeg with the only change that it returns ndarray dyrectly
// to avoid extra copying

#include "jpeg_decoder.h"
#include <setjmp.h>

#define VANILA
#include <../libjpeg/jpeglib.h>

struct my_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
	void (*emit_message) (j_common_ptr, int);
	boolean warning, stopOnWarning;
};
static void my_error_exit(j_common_ptr cinfo)
{
  my_error_mgr* myerr = (my_error_mgr *) cinfo->err;
  jpeg_destroy_decompress((jpeg_decompress_struct*)cinfo);
  throw runtime_error("Error reading file JPEG. JPEG code has signaled an error: %s", cinfo->err->jpeg_message_table[cinfo->err->msg_code]);
}

ndarray_uint8 decode_jpeg_vanila(void* data, size_t size)
{
	if (data == nullptr)
	{
		throw runtime_error("Error reading file JPEG. Got nullptr to decompress");
	}

	static jpeg_decompress_struct cinfo;
	static my_error_mgr jerr;
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

	{
		py::gil_scoped_release release;

		static bool initialized = false;
		if (!initialized)
		{
			cinfo.err = jpeg_std_error(&jerr.pub);
            jerr.pub.error_exit = my_error_exit;

			/* Now we can initialize the JPEG decompression object. */
			jpeg_create_decompress(&cinfo);
			initialized = true;
		}
		/* Step 2: specify data source (eg, a file) */
		jpeg_mem_src(&cinfo, (unsigned char*) data, size);
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
	}
	std::array<size_t, 3> shape = {cinfo.output_height, cinfo.output_width, 3};
	ndarray_uint8 ar(shape);
	unsigned char* ptr = (unsigned char*)ar.request().ptr;
	//buffer = (*cinfo.mem->alloc_sarray)
	//		((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
	{
		py::gil_scoped_release release;
		int i = 0;
		while (cinfo.output_scanline < cinfo.output_height)
		{
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
		//jpeg_destroy_decompress(&cinfo);
	}
	return ar;
}
