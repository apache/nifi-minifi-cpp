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

namespace org::apache::nifi::minifi::io {

RocksDbStream::RocksDbStream(std::string path, gsl::not_null<minifi::internal::RocksDatabase*> db, bool write_enable, minifi::internal::WriteBatch* batch, bool use_synchronous_writes)
    : BaseStreamImpl(),
      path_(std::move(path)),
      write_enable_(write_enable),
      db_(db),
      exists_([this] {
        auto opendb = db_->open();
        return opendb && opendb->Get(rocksdb::ReadOptions(), path_, &value_).ok();
      }()),
      offset_(0),
      batch_(batch),
      size_(value_.size()),
      use_synchronous_writes_(use_synchronous_writes) {
}

void RocksDbStream::close() {
}

void RocksDbStream::seek(size_t offset) {
  offset_ = offset;
}

size_t RocksDbStream::tell() const {
  return offset_;
}

size_t RocksDbStream::write(const uint8_t *value, size_t size) {
  if (!write_enable_) return STREAM_ERROR;
  if (size != 0 && IsNullOrEmpty(value)) return STREAM_ERROR;
  auto opendb = db_->open();
  if (!opendb) {
    return STREAM_ERROR;
  }
  rocksdb::Slice slice_value(reinterpret_cast<const char*>(value), size);
  rocksdb::Status status;
  size_ += size;
  if (batch_ != nullptr) {
    status = batch_->Merge(path_, slice_value);
  } else {
    rocksdb::WriteOptions opts;
    opts.sync = use_synchronous_writes_;
    status = opendb->Merge(opts, path_, slice_value);
  }
  if (status.ok()) {
    return size;
  } else {
    return STREAM_ERROR;
  }
}

size_t RocksDbStream::read(std::span<std::byte> buf) {
  // The check have to be in this order for RocksDBStreamTest "Read zero bytes" to succeed
  if (!exists_) return STREAM_ERROR;
  if (buf.empty()) return 0;
  if (offset_ >= value_.size()) return 0;

  const auto amtToRead = std::min(buf.size(), value_.size() - offset_);
  std::memcpy(buf.data(), value_.data() + offset_, amtToRead);
  offset_ += amtToRead;
  return amtToRead;
}

}  // namespace org::apache::nifi::minifi::io
