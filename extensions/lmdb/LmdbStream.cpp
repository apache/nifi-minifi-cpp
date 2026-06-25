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

#include "LmdbStream.h"

#include <cstring>
#include <algorithm>
#include <string>
#include <utility>

#include "io/validation.h"

namespace org::apache::nifi::minifi::extensions::lmdb {

LmdbStream::LmdbStream(std::string path, LmdbWrapper& lmdb_wrapper, bool write_enable)
    : BaseStreamImpl(),
      lmdb_wrapper_(lmdb_wrapper),
      path_(std::move(path)),
      write_enable_(write_enable),
      exists_(loadValue()) {}

bool LmdbStream::loadValue() {
  auto value_opt = lmdb_wrapper_.getValue(path_);
  if (!value_opt) {
    return false;
  }
  value_ = *value_opt;
  return true;
}

void LmdbStream::close() {
  commit();
}

bool LmdbStream::commit() {
  if (!write_enable_ || !dirty_) { return false; }

  if (!lmdb_wrapper_.putValue(path_, value_)) {
    return false;
  }
  dirty_ = false;
  return true;
}

void LmdbStream::seek(size_t offset) {
  offset_ = offset;
}

size_t LmdbStream::tell() const {
  return offset_;
}

size_t LmdbStream::write(const uint8_t* value, size_t size) {
  if (!write_enable_) { return io::STREAM_ERROR; }
  if (size != 0 && IsNullOrEmpty(value)) { return io::STREAM_ERROR; }
  value_.append(reinterpret_cast<const char*>(value), size);
  dirty_ = true;
  return size;
}

size_t LmdbStream::read(std::span<std::byte> buf) {
  if (!exists_) { return io::STREAM_ERROR; }
  if (buf.empty()) { return 0; }
  if (offset_ >= value_.size()) { return 0; }

  const auto bytes_to_read = std::min(buf.size(), value_.size() - offset_);
  std::memcpy(buf.data(), value_.data() + offset_, bytes_to_read);
  offset_ += bytes_to_read;
  return bytes_to_read;
}

}  // namespace org::apache::nifi::minifi::extensions::lmdb
