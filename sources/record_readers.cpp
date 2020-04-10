#include "record_readers.h"
#include <crc32c/crc32c.h>
#include <limits.h>
#include <cassert>
#include "common.h"


static const uint32_t kMaskDelta = 0xa282ead8ul;

inline uint32_t Mask(uint32_t crc)
{
	return ((crc >> 15) | (crc << 17)) + kMaskDelta;
}

inline uint32_t Unmask(uint32_t masked_crc)
{
	uint32_t rot = masked_crc - kMaskDelta;
	return ((rot >> 17) | (rot << 15));
}

RecordReader::RecordReader(fsal::File file): m_offset(0), m_file(std::move(file))
{
//	if (options.buffer_size > 0) {
//		input_stream_.reset(new BufferedInputStream(input_stream_.release(),
//		                                            options.buffer_size, true));
//	}
//	if (options.compression_type == RecordReaderOptions::ZLIB_COMPRESSION) {
//// We don't have zlib available on all embedded platforms, so fail.
//#if defined(IS_SLIM_BUILD)
//		LOG(FATAL) << "Zlib compression is unsupported on mobile platforms.";
//#else   // IS_SLIM_BUILD
//		input_stream_.reset(new ZlibInputStream(
//				input_stream_.release(), options.zlib_options.input_buffer_size,
//				options.zlib_options.output_buffer_size, options.zlib_options, true));
//#endif  // IS_SLIM_BUILD
//	} else if (options.compression_type == RecordReaderOptions::NONE) {
//		// Nothing to do.
//	} else {
//		LOG(FATAL) << "Unrecognized compression type :" << options.compression_type;
//	}
}

RecordReader::RecordReader(const std::string& file): m_offset(0)
{
	fsal::FileSystem fs;
	m_file = fs.Open(file);
}

fsal::Status RecordReader::ReadChecksummed(size_t offset, size_t size, uint8_t* dst)
{
	if (size >= SIZE_MAX - sizeof(uint32_t))
	{
		throw runtime_error("Record size too large %zd. Record file: %s", size, m_file.GetPath().c_str());
	}

	const size_t expected = size + sizeof(uint32_t);
	size_t result = 0;
	auto read_result = m_file.Read(dst, expected, &result);

	if (!read_result.ok() || read_result.is_eof())
	{
		if (result == 0 && read_result.is_eof())
		{
			return read_result;
		}

		throw runtime_error("Unexpected EOF. Corrupted record at offset %zd. Record file: %s", offset, m_file.GetPath().c_str());
	}

	const uint32_t masked_crc = *(uint32_t*)(dst + size);

	if (Unmask(masked_crc) != crc32c_value(dst, size))
	{
		throw runtime_error("Corrupted record. Error reading record at offset %zd. Record file: %s", offset, m_file.GetPath().c_str());
	}
	return true;
}


fsal::Status RecordReader::ReadRecord(size_t& offset, fsal::MemRefFile* mem_file)
{
	m_file.Seek(offset);

	RecordHeader header = { 0 };
	fsal::Status r = ReadChecksummed(offset, sizeof(RecordHeader::length), (uint8_t*)&header);
	if (!r.ok() || r.is_eof())
	{
		return r;
	}

	mem_file->Resize(header.length + sizeof(uint32_t));
	ReadChecksummed(offset + sizeof(RecordHeader), header.length, mem_file->GetDataPointer());

	offset += sizeof(RecordHeader) + header.length + sizeof(uint32_t);
	assert(offset == m_file.Tell());
	return true;
}

fsal::Status RecordReader::ReadRecord(size_t& offset, std::function<void*(size_t size)> alloc_func)
{
	m_file.Seek(offset);

	RecordHeader header = { 0 };
	fsal::Status r = ReadChecksummed(offset, sizeof(RecordHeader::length), (uint8_t*)&header);
	if (!r.ok() || r.is_eof())
	{
		return r;
	}

	uint8_t* data = (uint8_t*)alloc_func(header.length + sizeof(uint32_t));
	ReadChecksummed(offset + sizeof(RecordHeader), header.length, data);

	offset += sizeof(RecordHeader) + header.length + sizeof(uint32_t);
	assert(offset == m_file.Tell());
	return true;
}

fsal::Status RecordReader::GetNext()
{
	fsal::Status s = ReadRecord(m_offset, &m_mem_file);
	return s;
}

fsal::Status RecordReader::GetNext(std::function<void*(size_t size)> alloc_func)
{
	fsal::Status s = ReadRecord(m_offset, std::move(alloc_func));
	return s;
}

RecordReader::Metadata RecordReader::GetMetadata()
{
	if (m_metadata.file_size == -1)
	{
		m_file.Seek(0);
		m_metadata.data_size = 0;
		m_metadata.entries = 0;
		m_metadata.file_size = m_file.GetSize();

		uint64_t offset = 0;
		while (offset < m_metadata.file_size)
		{
			RecordHeader header = { 0 };
			ReadChecksummed(offset, sizeof(RecordHeader::length), (uint8_t*)&header);

			m_file.Seek(header.length + sizeof(uint32_t), fsal::File::CurrentPosition);
			offset += sizeof(RecordHeader) + header.length + sizeof(uint32_t);

			m_metadata.data_size += header.length;
			++m_metadata.entries;
		}
		m_metadata.file_size = m_metadata.data_size + (sizeof(RecordHeader) + sizeof(uint32_t)) * m_metadata.entries;
	}
	return m_metadata;
}


//#include "tensorflow/core/lib/core/coding.h"
//#include "tensorflow/core/lib/core/errors.h"
//#include "tensorflow/core/lib/hash/crc32c.h"
//#include "tensorflow/core/lib/io/buffered_inputstream.h"
//#include "tensorflow/core/lib/io/compression.h"
//#include "tensorflow/core/lib/io/random_inputstream.h"
//#include "tensorflow/core/platform/env.h"
//
//namespace tensorflow {
//	namespace io {
//
//		RecordReaderOptions RecordReaderOptions::CreateRecordReaderOptions(
//				const string& compression_type) {
//			RecordReaderOptions options;
//			if (compression_type == "ZLIB") {
//				options.compression_type = io::RecordReaderOptions::ZLIB_COMPRESSION;
//#if defined(IS_SLIM_BUILD)
//				LOG(ERROR) << "Compression is not supported but compression_type is set."
//               << " No compression will be used.";
//#else
//				options.zlib_options = io::ZlibCompressionOptions::DEFAULT();
//#endif  // IS_SLIM_BUILD
//			} else if (compression_type == compression::kGzip) {
//				options.compression_type = io::RecordReaderOptions::ZLIB_COMPRESSION;
//#if defined(IS_SLIM_BUILD)
//				LOG(ERROR) << "Compression is not supported but compression_type is set."
//               << " No compression will be used.";
//#else
//				options.zlib_options = io::ZlibCompressionOptions::GZIP();
//#endif  // IS_SLIM_BUILD
//			} else if (compression_type != compression::kNone) {
//				LOG(ERROR) << "Unsupported compression_type:" << compression_type
//				           << ". No compression will be used.";
//			}
//			return options;
//		}
//
//		RecordReader::RecordReader(RandomAccessFile* file,
//		                           const RecordReaderOptions& options)
//				: options_(options),
//				  input_stream_(new RandomAccessInputStream(file)),
//				  last_read_failed_(false) {
//			if (options.buffer_size > 0) {
//				input_stream_.reset(new BufferedInputStream(input_stream_.release(),
//				                                            options.buffer_size, true));
//			}
//			if (options.compression_type == RecordReaderOptions::ZLIB_COMPRESSION) {
//// We don't have zlib available on all embedded platforms, so fail.
//#if defined(IS_SLIM_BUILD)
//				LOG(FATAL) << "Zlib compression is unsupported on mobile platforms.";
//#else   // IS_SLIM_BUILD
//				input_stream_.reset(new ZlibInputStream(
//						input_stream_.release(), options.zlib_options.input_buffer_size,
//						options.zlib_options.output_buffer_size, options.zlib_options, true));
//#endif  // IS_SLIM_BUILD
//			} else if (options.compression_type == RecordReaderOptions::NONE) {
//				// Nothing to do.
//			} else {
//				LOG(FATAL) << "Unrecognized compression type :" << options.compression_type;
//			}
//		}
//
//// Read n+4 bytes from file, verify that checksum of first n bytes is
//// stored in the last 4 bytes and store the first n bytes in *result.
////
//// offset corresponds to the user-provided value to ReadRecord()
//// and is used only in error messages.
//		Status RecordReader::ReadChecksummed(uint64 offset, size_t n, tstring* result) {
//			if (n >= SIZE_MAX - sizeof(uint32)) {
//				return errors::DataLoss("record size too large");
//			}
//
//			const size_t expected = n + sizeof(uint32);
//			TF_RETURN_IF_ERROR(input_stream_->ReadNBytes(expected, result));
//
//			if (result->size() != expected) {
//				if (result->empty()) {
//					return errors::OutOfRange("eof");
//				} else {
//					return errors::DataLoss("truncated record at ", offset);
//				}
//			}
//
//			const uint32 masked_crc = core::DecodeFixed32(result->data() + n);
//			if (crc32c::Unmask(masked_crc) != crc32c::Value(result->data(), n)) {
//				return errors::DataLoss("corrupted record at ", offset);
//			}
//			result->resize(n);
//			return Status::OK();
//		}
//
//		Status RecordReader::GetMetadata(Metadata* md) {
//			if (!md) {
//				return errors::InvalidArgument(
//						"Metadata object call to GetMetadata() was null");
//			}
//
//			// Compute the metadata of the TFRecord file if not cached.
//			if (!cached_metadata_) {
//				TF_RETURN_IF_ERROR(input_stream_->Reset());
//
//				int64 data_size = 0;
//				int64 entries = 0;
//
//				// Within the loop, we always increment offset positively, so this
//				// loop should be guaranteed to either return after reaching EOF
//				// or encountering an error.
//				uint64 offset = 0;
//				tstring record;
//				while (true) {
//					// Read header, containing size of data.
//					Status s = ReadChecksummed(offset, sizeof(uint64), &record);
//					if (!s.ok()) {
//						if (errors::IsOutOfRange(s)) {
//							// We should reach out of range when the record file is complete.
//							break;
//						}
//						return s;
//					}
//
//					// Read the length of the data.
//					const uint64 length = core::DecodeFixed64(record.data());
//
//					// Skip reading the actual data since we just want the number
//					// of records and the size of the data.
//					TF_RETURN_IF_ERROR(input_stream_->SkipNBytes(length + kFooterSize));
//					offset += kHeaderSize + length + kFooterSize;
//
//					// Increment running stats.
//					data_size += length;
//					++entries;
//				}
//
//				cached_metadata_.reset(new Metadata());
//				cached_metadata_->stats.entries = entries;
//				cached_metadata_->stats.data_size = data_size;
//				cached_metadata_->stats.file_size =
//						data_size + (kHeaderSize + kFooterSize) * entries;
//			}
//
//			md->stats = cached_metadata_->stats;
//			return Status::OK();
//		}
//
//		Status RecordReader::ReadRecord(uint64* offset, tstring* record) {
//			// Position the input stream.
//			int64 curr_pos = input_stream_->Tell();
//			int64 desired_pos = static_cast<int64>(*offset);
//			if (curr_pos > desired_pos || curr_pos < 0 /* EOF */ ||
//			    (curr_pos == desired_pos && last_read_failed_)) {
//				last_read_failed_ = false;
//				TF_RETURN_IF_ERROR(input_stream_->Reset());
//				TF_RETURN_IF_ERROR(input_stream_->SkipNBytes(desired_pos));
//			} else if (curr_pos < desired_pos) {
//				TF_RETURN_IF_ERROR(input_stream_->SkipNBytes(desired_pos - curr_pos));
//			}
//			DCHECK_EQ(desired_pos, input_stream_->Tell());
//
//			// Read header data.
//			Status s = ReadChecksummed(*offset, sizeof(uint64), record);
//			if (!s.ok()) {
//				last_read_failed_ = true;
//				return s;
//			}
//			const uint64 length = core::DecodeFixed64(record->data());
//
//			// Read data
//			s = ReadChecksummed(*offset + kHeaderSize, length, record);
//			if (!s.ok()) {
//				last_read_failed_ = true;
//				if (errors::IsOutOfRange(s)) {
//					s = errors::DataLoss("truncated record at ", *offset);
//				}
//				return s;
//			}
//
//			*offset += kHeaderSize + length + kFooterSize;
//			DCHECK_EQ(*offset, input_stream_->Tell());
//			return Status::OK();
//		}
//
//		SequentialRecordReader::SequentialRecordReader(
//				RandomAccessFile* file, const RecordReaderOptions& options)
//				: underlying_(file, options), offset_(0) {}
//
//	}  // namespace io
//} // namespace tensorflow
