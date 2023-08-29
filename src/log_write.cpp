//
// Created by Shawn Zhao on 2023/8/29.
//

#include <cstdint>
#include "status.h"
#include "log_write.h"
namespace leveldb {
namespace log{
Status Writer::AddRecord(const Slice &slice) {
    const char* ptr = slice.data();
    size_t left = slice.size();

    Status s;
    bool begin = true;
    do {
        const int leftover = kBlockSize - block_offset_;
        assert(leftover >= 0);
        if (leftover < kHeaderSize) { // < 7
            if (leftover > 0) {
                static_assert(kHeaderSize == 7, "");
                dest_->Append(Slice("x00x00x00x00x00x00", leftover));
            }
            block_offset_ = 0;
        }
        assert(kBlockSize - block_offset_ - kHeaderSize >= 0);
        const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
        const size_t fragment_length = (left < avail) ? left : avail;

        RecordType type;
        const bool end = (left == fragment_length) ? left : avail;
        if (begin && end) {
            type = kFullType;
        }
        else if (begin) {
            type = kFirstType;
        }
        else if (end) {
            type = kLastType;
        }
        else {
            type = kMiddleType;
        }

        s = EmitPhysicalRecord(type, ptr, fragment_length);
        ptr += fragment_length;
        left -= fragment_length;
        begin = false;
    } while (s.ok() && left > 0);
    return s;
}

Status Writer::EmitPhysicalRecord(RecordType type, const char *ptr, size_t length) {
    assert(length <= 0xffff); // must fit in two bytes
    assert(block_offset_ + kHeaderSize + length <= kBlockSize);

    char buf[kHeaderSize];
    buf[4] = static_cast<char>(length & 0xff);
    buf[5] = static_cast<char>(length >> 8);
    buf[6] = static_cast<char>(type);

    uint32_t crc;
    EncodeFixed32(buf, crc);

    Statuc s = dest_->Append(Slice(buf, kHeaderSize));
    if (s.ok()) {
        s = dest_->Append(Slice(ptr, length));
        if (s.ok()) {
            s = data_->Flush();
        }
    }
    block_offset_ += kHeaderSize + length;
    return s;
}
} // namespace log
} // namespace leveldb