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
#include <algorithm>
#include <fstream>
#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <Exception.h>
#include "io/validation.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

RocksDbStream::RocksDbStream(std::string path, gsl::not_null<minifi::internal::RocksDatabase*> db, bool write_enable, rocksdb::WriteBatch* batch)
    : BaseStream(),
      path_(std::move(path)),
      write_enable_(write_enable),
      db_(db),
      batch_(batch),
      logger_(logging::LoggerFactory<RocksDbStream>::getLogger()) {
  auto opendb = db_->open();
  exists_ = opendb && opendb->Get(rocksdb::ReadOptions(), path_, &value_).ok();
  offset_ = 0;
  size_ = value_.size();
}

void RocksDbStream::close() {
}

void RocksDbStream::seek(size_t /*offset*/) {
  // noop
}

int RocksDbStream::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);
  if (!write_enable_) {
    return -1;
  }
  if (size == 0) {
    return 0;
  }
  if (!IsNullOrEmpty(value)) {
    auto opendb = db_->open();
    if (!opendb) {
      return -1;
    }
    rocksdb::Slice slice_value((const char *) value, size);
    rocksdb::Status status;
    size_ += size;
    if (batch_ != nullptr) {
      status = batch_->Merge(path_, slice_value);
    } else {
      rocksdb::WriteOptions opts;
      opts.sync = true;
      status = opendb->Merge(opts, path_, slice_value);
    }
    if (status.ok()) {
      return size;
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

size_t RocksDbStream::read(uint8_t *buf, size_t buflen) {
  // The check have to be in this order for RocksDBStreamTest "Read zero bytes" to succeed
  if (!exists_) return STREAM_ERROR;
  if (buflen == 0) return 0;
  if (IsNullOrEmpty(buf)) return STREAM_ERROR;
  if (offset_ >= value_.size()) return 0;

  const auto amtToRead = std::min(buflen, value_.size() - offset_);
  std::memcpy(buf, value_.data() + offset_, amtToRead);
  offset_ += amtToRead;
  return amtToRead;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

