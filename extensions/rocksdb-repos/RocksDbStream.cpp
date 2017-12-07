/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "RocksDbStream.h"
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include "io/validation.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

RocksDbStream::RocksDbStream(const std::string &path, rocksdb::DB *db, bool write_enable)
    : BaseStream(),
      path_(path),
      write_enable_(write_enable),
      db_(db),
      logger_(logging::LoggerFactory<RocksDbStream>::getLogger()) {
  rocksdb::Status status;
  status = db_->Get(rocksdb::ReadOptions(), path_, &value_);
  if (status.ok()) {
    exists_ = true;
  } else {
    exists_ = false;
  }
  offset_ = 0;
  size_ = value_.size();
}

void RocksDbStream::closeStream() {
}

void RocksDbStream::seek(uint64_t offset) {
  // noop
}

int RocksDbStream::writeData(std::vector<uint8_t> &buf, int buflen) {
  if (buf.capacity() < buflen) {
    return -1;
  }
  return writeData(reinterpret_cast<uint8_t *>(&buf[0]), buflen);
}

// data stream overrides

int RocksDbStream::writeData(uint8_t *value, int size) {
  if (!IsNullOrEmpty(value) && write_enable_) {
    rocksdb::Slice slice_value((const char *) value, size);
    rocksdb::Status status;
    size_ += size;
    rocksdb::WriteOptions opts;
    opts.sync = true;
    db_->Merge(opts, path_, slice_value);
    if (status.ok()) {
      return 0;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

template<typename T>
inline std::vector<uint8_t> RocksDbStream::readBuffer(const T& t) {
  std::vector<uint8_t> buf;
  buf.resize(sizeof t);
  readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
  return buf;
}

int RocksDbStream::readData(std::vector<uint8_t> &buf, int buflen) {
  if (buf.capacity() < buflen) {
    buf.resize(buflen);
  }
  int ret = readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen);

  if (ret < buflen) {
    buf.resize(ret);
  }
  return ret;
}

int RocksDbStream::readData(uint8_t *buf, int buflen) {
  if (!IsNullOrEmpty(buf) && exists_) {
    int amtToRead = buflen;
    if (offset_ >= value_.size()) {
      return 0;
    }
    if (buflen > value_.size() - offset_) {
      amtToRead = value_.size() - offset_;
    }
    std::memcpy(buf, value_.data() + offset_, amtToRead);
    offset_ += amtToRead;
    return amtToRead;
  } else {
    return -1;
  }
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

