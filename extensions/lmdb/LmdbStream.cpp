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

namespace org::apache::nifi::minifi::io {

LmdbStream::LmdbStream(std::string path, MDB_env* lmdb_env, MDB_dbi* lmdb_handle, bool write_enable)
    : BaseStreamImpl(),
      path_(std::move(path)),
      write_enable_(write_enable),
      lmdb_env_(lmdb_env),
      lmdb_handle_(lmdb_handle),
      exists_(loadValue()) {}

bool LmdbStream::loadValue() {
  MDB_val key{path_.size(), const_cast<char*>(path_.data())};
  MDB_val value{};

  MDB_txn* txn = nullptr;
  if (const int rc = mdb_txn_begin(lmdb_env_, nullptr, MDB_RDONLY, &txn); rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB read transaction in loadValue: {}", mdb_strerror(rc));
    return false;
  }
  auto guard = gsl::finally([txn] { mdb_txn_abort(txn); });

  const auto rc = mdb_get(txn, *lmdb_handle_, &key, &value);
  if (rc == MDB_SUCCESS) {
    value_ = std::string(static_cast<char*>(value.mv_data), value.mv_size);
    return true;
  } else if (rc != MDB_NOTFOUND) {
    logger_->log_error("Failed to get value from LMDB database: {}", mdb_strerror(rc));
  }
  return false;
}

void LmdbStream::close() {
  commit();
}

bool LmdbStream::commit() {
  if (!write_enable_ || !dirty_) { return false; }

  MDB_txn* txn = nullptr;
  auto rc = mdb_txn_begin(lmdb_env_, nullptr, 0, &txn);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to begin LMDB transaction in close: {}", mdb_strerror(rc));
    return false;
  }

  MDB_val key{path_.size(), const_cast<char*>(path_.data())};
  MDB_val val{value_.size(), const_cast<char*>(value_.data())};
  rc = mdb_put(txn, *lmdb_handle_, &key, &val, 0);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to put value in LMDB database during close: {}", mdb_strerror(rc));
    mdb_txn_abort(txn);
    return false;
  }

  rc = mdb_txn_commit(txn);
  if (rc != MDB_SUCCESS) {
    logger_->log_error("Failed to commit LMDB transaction during close: {}", mdb_strerror(rc));
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
  if (!write_enable_) { return STREAM_ERROR; }
  if (size != 0 && IsNullOrEmpty(value)) { return STREAM_ERROR; }
  value_.append(reinterpret_cast<const char*>(value), size);
  dirty_ = true;
  return size;
}

size_t LmdbStream::read(std::span<std::byte> buf) {
  if (!exists_) { return STREAM_ERROR; }
  if (buf.empty()) { return 0; }
  if (offset_ >= value_.size()) { return 0; }

  const auto bytes_to_read = std::min(buf.size(), value_.size() - offset_);
  std::memcpy(buf.data(), value_.data() + offset_, bytes_to_read);
  offset_ += bytes_to_read;
  return bytes_to_read;
}

}  // namespace org::apache::nifi::minifi::io
