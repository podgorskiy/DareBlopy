#pragma once
#include "fsal_common.h"
#include "FileInterface.h"

#include <cstdio>
#include <memory>
#include <string.h>
#include "zlib.h"
#include "common.h"


namespace fsal
{
	class ZlibFile : public FileInterface
	{
		enum
		{
			buff_size = 256 * 1024,
			buff_real_size = buff_size * 8
		};

	public:
		ZlibFile(const std::shared_ptr<FileInterface>& compressed, int window = MAX_WBITS)
		{
			m_file = compressed;
			m_input_buff.reset(new uint8_t[buff_real_size]);
			m_output_buff.reset(new uint8_t[buff_real_size]);
			m_inputbuff_begin = 0;
			m_inputbuff_end = 0;
			m_outputbuff_begin = 0;
			m_outputbuff_end = 0;
			m_src_size = m_file->GetSize();
			m_src_offset = 0;
			m_dst_offset = 0;
			m_window = window;

            m_stream.zalloc = Z_NULL;
			m_stream.zfree = Z_NULL;
			m_stream.opaque = Z_NULL;
			m_stream.next_in = Z_NULL;
			m_stream.avail_in = 0;

            int status = inflateInit2(&m_stream, m_window);
			if (status != Z_OK)
				throw runtime_error("Failed to init zlib");
		}

		~ZlibFile() {};

		bool ok() const { return m_file != nullptr; }

		path GetPath() const { return m_file->GetPath(); }

		Status Open(path filepath, Mode mode) { return Status::kFailed; }

		Status ReadData(uint8_t* dst, size_t size, size_t* bytesRead)
		{
			*bytesRead = 0;
			while (size > 0)
			{
				size_t data_avail = m_outputbuff_end - m_outputbuff_begin;
				if (data_avail > 0)
				{
					size_t will_write_out = std::min(data_avail, size);
					if (dst != nullptr)
					{
						memcpy(dst, m_output_buff.get() + m_outputbuff_begin, will_write_out);
						dst += will_write_out;
					}
					size -= will_write_out;
					*bytesRead += will_write_out;
					m_dst_offset += will_write_out;
					m_outputbuff_begin += will_write_out;
				}
				if (size == 0)
					return Status::kOk;
				if (m_inputbuff_end + buff_size >= buff_real_size)
				{
					memmove(m_input_buff.get(), m_input_buff.get() + m_inputbuff_begin,
					       m_inputbuff_end - m_inputbuff_begin);
					m_inputbuff_end -= m_inputbuff_begin;
					m_inputbuff_begin = 0;
				}
				if (m_outputbuff_end + buff_size >= buff_real_size)
				{
					memmove(m_output_buff.get(), m_output_buff.get() + m_outputbuff_begin,
					       m_outputbuff_end - m_outputbuff_begin);
					m_outputbuff_end -= m_outputbuff_begin;
					m_outputbuff_begin = 0;
				}
				size_t bytes_to_read = std::min(m_src_size - m_src_offset, size_t(buff_size));
				size_t _bytesRead = bytes_to_read;
				Status status = m_file->ReadData(m_input_buff.get() + m_inputbuff_end, bytes_to_read, &_bytesRead);
				m_inputbuff_end += _bytesRead;
				m_src_offset += _bytesRead;

				m_stream.next_in = m_input_buff.get() + m_inputbuff_begin;
				m_stream.next_out = m_output_buff.get() + m_outputbuff_end;
				m_stream.avail_in = m_inputbuff_end - m_inputbuff_begin;
				m_stream.avail_out = buff_size;

				int result = inflate(&m_stream, Z_NO_FLUSH);
				if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
				{
					throw runtime_error("Inflate failed: %s", m_stream.msg);
				}
				m_outputbuff_end = m_stream.next_out - m_output_buff.get();
				m_inputbuff_begin = m_stream.next_in - m_input_buff.get();

				if (m_src_offset == m_src_size && m_outputbuff_end == m_outputbuff_begin && m_inputbuff_end == m_inputbuff_begin)
				{
					return Status::kEOF;
				}
			}
		}

		Status WriteData(const uint8_t* src, size_t size) { return Status::kFailed; }

		Status _SetPosition(size_t position)
		{
			if (position > m_dst_offset)
			{
				size_t _bytesRead;
				ReadData(nullptr, position - m_dst_offset, &_bytesRead);

				if (position != m_dst_offset)
					return Status::kFailed;
				return Status::kOk;
			}

            inflateEnd(&m_stream);

            m_stream.zalloc = Z_NULL;
			m_stream.zfree = Z_NULL;
			m_stream.opaque = Z_NULL;
			m_stream.next_in = Z_NULL;
			m_stream.avail_in = 0;
			m_inputbuff_begin = 0;
			m_inputbuff_end = 0;
			m_outputbuff_begin = 0;
			m_outputbuff_end = 0;
			m_src_offset = 0;
			m_dst_offset = 0;
			m_file->SetPosition(0);

            int status = inflateInit2(&m_stream, m_window);
			if (status != Z_OK)
				throw runtime_error("Failed to init zlib");

			if (position != 0)
			{
				size_t _bytesRead;
				ReadData(nullptr, position, &_bytesRead);

				if (_bytesRead != position)
					return Status::kFailed;
			}

			return Status::kOk;
		}

		Status SetPosition(size_t position) const override
		{
			return const_cast<ZlibFile*>(this)->_SetPosition(position);
		}

		size_t GetPosition() const override { return m_dst_offset; }

		size_t GetSize() const override { return -1; }

		Status FlushBuffer() const override { return Status::kFailed; }

		uint64_t GetLastWriteTime() const override { return 0; }

		const uint8_t* GetDataPointer() const override { return nullptr; }

		uint8_t* GetDataPointer()  override { return nullptr; }

		bool Resize(size_t newSize) { return false; }

	private:
		std::shared_ptr<FileInterface> m_file;
		size_t m_src_size;
		size_t m_src_offset;
		size_t m_dst_offset;
		z_stream m_stream = {nullptr};
		int m_window = MAX_WBITS;

		std::shared_ptr<uint8_t> m_input_buff;
		std::shared_ptr<uint8_t> m_output_buff;
		size_t m_inputbuff_begin;
		size_t m_inputbuff_end;
		size_t m_outputbuff_begin;
		size_t m_outputbuff_end;
	};
}
