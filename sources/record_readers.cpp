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
	// Does not handle compression yet
	if (!m_file)
		throw runtime_error("Can't create RecordReader. Given file is None");
}

RecordReader::RecordReader(const std::string& file): m_offset(0)
{
	fsal::FileSystem fs;
	m_file = fs.Open(file);
	if (!m_file)
		throw runtime_error("Can't create RecordReader. Can't find file %", file.c_str());
	// Does not handle compression yet
}

fsal::Status RecordReader::ReadChecksummed(size_t offset, size_t size, uint8_t* dst)
{
	if (size >= SIZE_MAX - sizeof(uint32_t))
	{
		throw runtime_error("Record size too large %zd. Record file: %s", size, m_file.GetPath().c_str());
	}

	const size_t expected = size + sizeof(uint32_t);    // reading data together with crc32.
	                                                    // Preallocated buffer has sizeof(uint32) padding
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
	*(uint32_t*)(dst + size) = 0;

	if (Unmask(masked_crc) != crc32c_value(dst, size))
	{
		throw runtime_error("Corrupted record, CRC32 didn't match. Error reading record at offset %zd. Record file: %s", offset, m_file.GetPath().c_str());
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

	auto* data = (uint8_t*)alloc_func(header.length);
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
