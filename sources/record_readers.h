#pragma once
#include <inttypes.h>
#include <memory>
#include <fsal.h>
#include <MemRefFile.h>
#include <bfio.h>


#pragma pack(push,1)
struct RecordHeader
{
	uint64_t length;
	uint32_t crc_of_length;
};
#pragma pack(pop)


class RecordReader
{
public:
	struct Metadata
	{
		int64_t file_size = -1;
		int64_t data_size = -1;
		int64_t entries = -1;
	};

	explicit RecordReader(fsal::File file);

	explicit RecordReader(const std::string& file);

	virtual ~RecordReader() = default;

	fsal::Status ReadRecord(size_t& offset, fsal::MemRefFile* mem_file);

	fsal::Status ReadRecord(size_t& offset, std::function<void*(size_t size)> alloc_func);

	Metadata GetMetadata();

	fsal::Status GetNext();

	fsal::Status GetNext(std::function<void*(size_t size)> alloc_func);

	const fsal::MemRefFile& record() const { return m_mem_file; }

	uint64_t offset() const { return m_offset; }

private:
	fsal::Status ReadChecksummed(size_t offset, size_t size, uint8_t* data);
	fsal::MemRefFile m_mem_file;
	uint64_t m_offset;
	fsal::File m_file;
	Metadata m_metadata;
};
