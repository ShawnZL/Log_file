# levelDB log系统的解析

自己整理版本，终于看懂了一些，之后继续看，加深印象

# log文件写入

```c++
// 这里是一个block 32KB
kHeader(0-3 checksum 4-5 length 6 type)(7B)| 写入数据库的数据
```

**注意这个地方，每一个块都要以`kHeader`开始，然后后边接我们要写的数据，这个kHeader记录我们本次的类型（后边的数据是开始还是中间还是结束），数据长度等信息，**

这段代码是 LevelDB 中用于将记录添加到数据库的写入操作代码。它实现了将一个大的数据块分割成多个物理记录，并按照指定的格式写入到数据库中。

写入操作的格式如下：

1. 如果剩余的块空间不足以容纳一个记录头部（`kHeaderSize` 字节），则将剩余空间填充为零，重置块的偏移量为 0。

2. 确定当前记录片段的长度，即本次写入的数据的长度，根据当前块的剩余空间和记录的长度。

3. 根据记录的位置确定记录的类型：
   - 如果是记录的起始和结束，则类型为 `kFullType`。
   - 如果是记录的起始但不是结束，则类型为 `kFirstType`。
   - 如果不是记录的起始但是结束，则类型为 `kLastType`。
   - 如果既不是记录的起始也不是结束，则类型为 `kMiddleType`。

4. 使用 `EmitPhysicalRecord` 函数将物理记录写入数据库。

5. 更新指针和剩余长度，将 `begin` 标志设置为 `false`，以便下次循环时知道不是记录的起始。

6. 重复步骤 2 到步骤 5，直到所有的数据被写入到数据库。

`EmitPhysicalRecord` 函数实现了将一个物理记录写入到数据库的过程，它的格式如下：

1. 确保记录长度不超过 0xFFFF，即两个字节可以表示的最大值。

2. 确保写入的数据长度不会超过当前块的剩余空间。

3. 构建记录头部（7 字节）：前 4 个字节为记录长度的低字节，接下来的 2 个字节为记录长度的高字节，最后一个字节为记录类型。

4. 计算记录头部的 CRC 校验和。

5. 调用 `dest_->Append` 函数将记录头部写入到数据库。

6. 调用 `dest_->Append` 函数将记录的实际数据写入到数据库。

7. 调用 `data_->Flush()` 函数刷新数据到磁盘。

8. 更新块的偏移量。

这个写入操作的格式保证了记录的完整性和一致性，以及记录的类型和数据的正确性。每个记录都以一个记录头部开始，记录头部包含了记录的长度和类型信息，然后是记录的实际数据。这样的格式可以让 LevelDB 在读取时能够准确地解析和恢复出原始的数据。

# log读取

这段代码是 LevelDB 中用于读取日志记录的 `Reader` 类的实现部分。LevelDB 是一个用于存储和检索键值对数据的开源数据库库，其中的日志记录用于持久化存储操作。

以下是这段代码的主要逻辑：

1. 构造函数：构造一个 `Reader` 对象。它接受一个文件指针、一个 `Reporter` 对象（用于报告错误）、一个是否开启校验和的标志和初始偏移量作为参数。初始化成员变量并分配一块内存作为缓冲区。

2. `SkipToInitialBlock` 函数：用于跳过文件中的一些字节，以定位到包含初始记录的块。如果需要跳过，它会执行相应的跳过操作。

3. `ReadRecord` 函数：用于读取一个日志记录。它在循环中读取物理记录，将其拼接成逻辑记录，根据记录类型返回相应的值。

4. `ReadPhysicalRecord` 函数：从缓冲区中读取物理记录。它处理不同的记录类型，解析记录的头部，验证校验和等。它会返回记录的类型以及记录的内容。

这段代码的功能是从 LevelDB 的日志文件中读取日志记录，包括处理物理记录的类型、校验和以及记录的拼接等。它是 LevelDB 数据库引擎的核心部分之一，用于实现持久化存储和数据恢复。

## 函数

从而定位到包含初始记录的块的偏移位置，也就是最后一个块，以便后续读取操作。

```c++
bool Reader::SkipToInitialBlock() {
  const size_t offset_in_block = initial_offset_ % kBlockSize; // 这是初始偏移量在块内的偏移量。通过取 initial_offset_ 对 kBlockSize 取模，我们可以确定初始偏移量相对于块的起始位置的偏移量。
  uint64_t block_start_location = initial_offset_ - offset_in_block; // 这是计算出的包含初始记录的块的偏移位置。通过减去 offset_in_block，我们得到了块的偏移位置。

  // Don't search a block if we'd be in the trailer
  if (offset_in_block > kBlockSize - 6) {
    block_start_location += kBlockSize; //如果确实在块的末尾附近，就将 block_start_location 增加一个块的大小，以跳过当前块，定位到下一个块的起始位置。
  }

  end_of_buffer_offset_ = block_start_location;

  // Skip to start of first block that can contain the initial record
  if (block_start_location > 0) {
    Status skip_status = file_->Skip(block_start_location);
    if (!skip_status.ok()) {
      ReportDrop(block_start_location, skip_status);
      return false;
    }
  }

  return true;
}
```

**读取函数**

其中的一些对应关系

fragment是`ReadPhysicalRecord`解析的信息，头部信息已经验证完毕，**只有长度和type是我们关注的**，只包含数据（用长度来提取），**不包含头部信息，** type直接进行返回。

```c++
prospective_record_offset = physical_record_offset;
这行代码的目的是在读取物理记录的过程中，将计算得到的物理记录的起始位置（physical_record_offset）赋值给 prospective_record_offset 变量。这个变量用于记录当前逻辑记录的位置（或者说文件中记录的位置），以便在处理不同类型的记录时能够正确地更新 last_record_offset_。这是为了保持记录的顺序和正确性。

在 ReadRecord 函数中，当处理不同类型的物理记录时，prospective_record_offset 被设置为正在处理的物理记录的起始位置。在确定当前逻辑记录的位置后，它被赋值给 last_record_offset_，以便在需要时可以在文件中进行位置跳转或恢复。

总之，prospective_record_offset 的作用是在读取物理记录的过程中保持对当前逻辑记录位置的正确记录，以便在处理分段、错误恢复等情况时能够准确地维护记录的顺序和位置。
```



```c++
bool Reader::ReadRecord(Slice* record, std::string* scratch) {
  if (last_record_offset_ < initial_offset_) { // 这两个变量用于跟踪已经读取的记录的偏移量和期望读取的初始记录的偏移量。
    if (!SkipToInitialBlock()) {
      return false;
    }
  }

  scratch->clear();
  record->clear();
  bool in_fragmented_record = false; // 这个布尔值用于标记是否正在读取分段记录。
  // Record offset of the logical record that we're reading
  // 0 is a dummy value to make compilers happy
  uint64_t prospective_record_offset = 0; // 这个变量用于记录当前逻辑记录的偏移量。

  Slice fragment;
  while (true) {
    const unsigned int record_type = ReadPhysicalRecord(&fragment); // 这个函数用于读取物理记录，返回物理记录的类型。fragment是读取完成的一条物理记录信息，也就是一个分块，但是不包括头部信息

    // ReadPhysicalRecord may have only had an empty trailer remaining in its
    // internal buffer. Calculate the offset of the next physical record now
    // that it has returned, properly accounting for its header size.
    uint64_t physical_record_offset =
        end_of_buffer_offset_ - buffer_.size() - kHeaderSize - fragment.size();
    // 从最后一个记录块的地址开始读取数据，首先获取
		// buffer_.size() 是当前缓冲区中的数据大小。在 ReadPhysicalRecord 函数中，数据会从文件读取到缓冲区中，然后逐步从缓冲区中解析。
    // kHeaderSize 是物理记录的头部大小，包括记录类型和长度信息。每个物理记录的开头都包含这个头部信息。
    // fragment.size() 是当前正在处理的片段（部分记录）的大小。在分段的情况下，一个完整的逻辑记录可能被分为多个片段。
    // 将这些信息放在一起，end_of_buffer_offset_ - buffer_.size() 表示已读取数据的文件位置减去当前缓冲区中的数据大小，得到的结果是缓冲区前面的未读取数据的位置。从这个未读取数据的位置减去 kHeaderSize，得到的位置应该是正在处理的物理记录的头部位置。从头部位置减去 fragment.size()，得到的位置就是正在处理的物理记录的起始位置。下边也就是头部信息
    if (resyncing_) {
      if (record_type == kMiddleType) {
        continue;
      } else if (record_type == kLastType) {
        resyncing_ = false;
        continue;
      } else {
        resyncing_ = false;
      }
    }

    switch (record_type) {
      case kFullType: // 如果当前正在读取分段记录，则表示错误，如果不是，则记录下一次逻辑记录的偏移量，并将 record 设置为读取到的记录。然后返回 true 表示读取成功。
        if (in_fragmented_record) {
          // Handle bug in earlier versions of log::Writer where
          // it could emit an empty kFirstType record at the tail end
          // of a block followed by a kFullType or kFirstType record
          // at the beginning of the next block.
          if (!scratch->empty()) {
            ReportCorruption(scratch->size(), "partial record without end(1)");
          }
        }
        prospective_record_offset = physical_record_offset; // 记录下一次的头部文件
        scratch->clear();
        *record = fragment;
        last_record_offset_ = prospective_record_offset;
        return true;

      case kFirstType: // 如果当前正在读取分段记录，则表示错误，否则记录下一次逻辑记录的偏移量，将 scratch 设置为读取到的记录的内容，并标记正在读取分段记录。
        if (in_fragmented_record) {
          // Handle bug in earlier versions of log::Writer where
          // it could emit an empty kFirstType record at the tail end
          // of a block followed by a kFullType or kFirstType record
          // at the beginning of the next block.
          if (!scratch->empty()) {
            ReportCorruption(scratch->size(), "partial record without end(2)");
          }
        }
        prospective_record_offset = physical_record_offset;
        scratch->assign(fragment.data(), fragment.size());
        in_fragmented_record = true;
        break;

      case kMiddleType: // 如果没有在分段记录中读取到内容，表示错误，否则将读取到的内容追加到 scratch 中。
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(1)");
        } else {
          scratch->append(fragment.data(), fragment.size());
        }
        break;

      case kLastType: // 如果不在分段记录中读取内容，表示错误，否则将读取到的内容追加到 scratch 中，将 record 设置为 scratch 的内容，记录下一次逻辑记录的偏移量，然后返回 true 表示读取成功。
        if (!in_fragmented_record) {
          ReportCorruption(fragment.size(),
                           "missing start of fragmented record(2)");
        } else {
          scratch->append(fragment.data(), fragment.size());
          *record = Slice(*scratch);
          last_record_offset_ = prospective_record_offset;
          return true;
        }
        break;

      case kEof:
        if (in_fragmented_record) {
          // This can be caused by the writer dying immediately after
          // writing a physical record but before completing the next; don't
          // treat it as a corruption, just ignore the entire logical record.
          scratch->clear();
        }
        return false;

      case kBadRecord:
        if (in_fragmented_record) {
          ReportCorruption(scratch->size(), "error in middle of record");
          in_fragmented_record = false;
          scratch->clear();
        }
        break;

      default: {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "unknown record type %u", record_type);
        ReportCorruption(
            (fragment.size() + (in_fragmented_record ? scratch->size() : 0)),
            buf);
        in_fragmented_record = false;
        scratch->clear();
        break;
      }
    }
  }
  return false;
}
```

最后一个读取函数

当 `ReadPhysicalRecord` 函数成功解析出**一条物理记录的内容后，**它将这个内容存储在 `result` 所指向的 `Slice` 对象中，以便在函数外部使用。`Slice` 对象通常包含一个指向数据的指针和数据的长度，允许在不进行数据复制的情况下对数据进行引用和操作。



1. `buffer_`：这是用于存储从文件中读取的数据的缓冲区。
2. `eof_`：这是一个布尔值，用于标记是否已经到达文件的末尾。
3. `kHeaderSize`：这是头部的固定大小，用于解析记录的头部。
4. 解析头部：从缓冲区中读取记录的头部信息，包括类型和长度。将类型和长度信息解析出来，以及记录的内容长度。
5. 判断缓冲区是否为空：如果缓冲区中的数据不足以解析记录的头部，则可能需要从文件中继续读取数据。在这里进行判断，如果缓冲区中的数据不足，且不是已经到达文件末尾，就尝试从文件中读取更多数据。
6. 解析CRC校验：如果需要进行CRC校验，解析记录头部中的CRC校验值，并计算记录内容的CRC校验值，进行比较。如果校验不通过，表示记录可能已损坏，报告错误。
7. 移除已读取的数据：移除缓冲区中已读取的记录，以便下次读取新的记录。
8. 跳过未在初始偏移量之后开始的记录：如果当前记录的偏移量小于期望的初始偏移量，则清空 `result` 并返回 `kBadRecord`，表示该记录需要被跳过。
9. 返回记录类型：根据解析出来的记录类型，返回对应的类型值。

这个函数的主要功能是从缓冲区中读取物理记录的头部和内容，解析记录类型、长度和CRC校验，跳过不需要的记录，并将解析出来的记录内容存储在 `result` 中返回。

