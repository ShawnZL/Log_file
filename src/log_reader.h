//
// Created by Shawn Zhao on 2023/8/29.
//

#ifndef LOG_FILE_LOG_READER_H
#define LOG_FILE_LOG_READER_H

#include <cstdint>

#include "log_format.h"
#include "slice.h"
#include "status.h"

namespace leveldb{
class SequentialFile;
namespace log {
class Reader {
public:
    class Reporter {
    public:
        virtual ~Reporter();

        virtual void Corruption(size_t bytes, const Status& status) = 0;
    };

    Reader(SequentialFile* file, Reporter* reporter, bool checksum,
           uint64_t initial_offset);

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    ~Reader();

    bool ReadRecord(Slice* record, std::string* scratch);

    // Returns the physical offset of the last record returned by ReadRecord.
    //
    // Undefined before the first call to ReadRecord.
    uint64_t LastRecordOffset();

private:
    // Extend record types with the following special values
    enum {
        kEof = kMaxRecordType + 1,
        // Returned whenever we find an invalid physical record.
        // Currently there are three situations in which this happens:
        // * The record has an invalid CRC (ReadPhysicalRecord reports a drop)
        // * The record is a 0-length record (No drop is reported)
        // * The record is below constructor's initial_offset (No drop is reported)
        kBadRecord = kMaxRecordType + 2
    };

    bool SkipToInitialBlock();

    // Return type, or one of the preceding special values
    unsigned int ReadPhysicalRecord(Slice* result);

    // Reports dropped bytes to the reporter.
    // buffer_ must be updated to remove the dropped bytes prior to invocation.
    void ReportCorruption(uint64_t bytes, const char* reason);
    void ReportDrop(uint64_t bytes, const Status& reason);

    SequentialFile* const file_;
    Reporter* const reporter_;
    bool const checksum_;
    char* const backing_store_;
    Slice buffer_;
    bool eof_;  // Last Read() indicated EOF by returning < kBlockSize

    // Offset of the last record returned by ReadRecord.
    uint64_t last_record_offset_;
    // Offset of the first location past the end of buffer_.
    uint64_t end_of_buffer_offset_;

    // Offset at which to start looking for the first record to return
    uint64_t const initial_offset_;

    // True if we are resynchronizing after a seek (initial_offset_ > 0). In
    // particular, a run of kMiddleType and kLastType records can be silently
    // skipped in this mode
    bool resyncing_;
};
} // namespace log
} // namespace leveldb

#endif //LOG_FILE_LOG_READER_H
