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
	RecordReader(const RecordReader&) = delete; // non construction-copyable
	RecordReader& operator=( const RecordReader&) = delete; // non copyable

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
