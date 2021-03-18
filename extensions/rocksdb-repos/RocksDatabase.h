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

#pragma once

#include <mutex>

#include "utils/OptionalUtils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"
#include "logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

class RocksDatabase;

// Not thread safe
class OpenRocksDB {
  friend class RocksDatabase;

  OpenRocksDB(RocksDatabase& db, gsl::not_null<std::shared_ptr<rocksdb::DB>> impl);

 public:
  OpenRocksDB(const OpenRocksDB&) = delete;
  OpenRocksDB(OpenRocksDB&&) noexcept = default;
  OpenRocksDB& operator=(const OpenRocksDB&) = delete;
  OpenRocksDB& operator=(OpenRocksDB&&) = default;

  rocksdb::Status Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  rocksdb::Status Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value);

  std::vector<rocksdb::Status> MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values);

  rocksdb::Status Write(const rocksdb::WriteOptions& options, rocksdb::WriteBatch* updates);

  rocksdb::Status Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key);

  rocksdb::Status Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  bool GetProperty(const rocksdb::Slice& property, std::string* value);

  std::unique_ptr<rocksdb::Iterator> NewIterator(const rocksdb::ReadOptions& options);

  rocksdb::Status NewCheckpoint(rocksdb::Checkpoint** checkpoint);

  rocksdb::Status FlushWAL(bool sync);

  rocksdb::DB* get();

 private:
  gsl::not_null<RocksDatabase*> db_;
  gsl::not_null<std::shared_ptr<rocksdb::DB>> impl_;
};

class RocksDatabase {
  friend class OpenRocksDB;

 public:
  enum class Mode {
    ReadOnly,
    ReadWrite
  };

  RocksDatabase(const rocksdb::Options& options, const std::string& name, Mode mode = Mode::ReadWrite);

  utils::optional<OpenRocksDB> open();

 private:
  /*
   * notify RocksDatabase that the next open should check if they can reopen the database
   * until a successful reopen no more open is possible
   */
  void invalidate();

  const rocksdb::Options open_options_;
  const std::string db_name_;
  const Mode mode_;

  std::mutex mtx_;
  std::shared_ptr<rocksdb::DB> impl_;

  static std::shared_ptr<core::logging::Logger> logger_;
};

} /* namespace internal */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
