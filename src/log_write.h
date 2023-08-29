//
// Created by Shawn Zhao on 2023/8/29.
//

#ifndef LOG_FILE_LOG_WRITE_H
#define LOG_FILE_LOG_WRITE_H

#include <iostream>
#include "status.h"
#include "log_format.h"
namespace leveldb {
class WritableFile;

namespace log {
class Writer {
public:
    explicit Writer (WritableFile* dest);

    Writer (WritableFile* dest, uint64_t dest_length);

    Writer (const Writer&) = delete;
    Writer& operator=(const Writer &) = delete;

    ~Writer();

    Status AddRecord(const Slice& slice);

private:
    Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

    WritableFile* dest_;
    int block_offset_;  // Current offset in block

    // crc32c values for all supported record types.  These are
    // pre-computed to reduce the overhead of computing the crc of the
    // record type stored in the header.
    uint32_t type_crc_[kMaxRecordType + 1];
};
} // namespace log
} // leveldb

#endif //LOG_FILE_LOG_WRITE_H
