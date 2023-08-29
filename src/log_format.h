//
// Created by Shawn Zhao on 2023/8/29.
//

#ifndef LOG_FILE_LOG_FORMAT_H
#define LOG_FILE_LOG_FORMAT_H

namespace leveldb {
namespace log {
enum RecordType {
    // zero is reserved fro preallocated files
    kZeroType = 0,
    kFullType = 1,

    // for fragments
    kFirstType = 2,
    kMiddleType = 3,
    kLastType = 4
};
static const int kMaxRecordType = kLastType;

static const int kBlockSize = 32768; // 32KB

// Header is checksum(4) length(2), type(1)
static const int kHeaderSize = 4 + 2 + 1;

}
}
#endif //LOG_FILE_LOG_FORMAT_H
