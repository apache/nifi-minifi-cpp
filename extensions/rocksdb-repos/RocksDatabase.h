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

#include "utils/OptionalUtils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

class RocksDatabase;

// Not thread safe
class OpenRocksDB {
  friend class RocksDatabase;

  OpenRocksDB(RocksDatabase& db, gsl::not_null<rocksdb::DB*> impl);

 public:
  OpenRocksDB(OpenRocksDB&& other);

  OpenRocksDB& operator=(OpenRocksDB&& other);

  rocksdb::Status Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  rocksdb::Status Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value);

  std::vector<rocksdb::Status> MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values);

  rocksdb::Status Write(const rocksdb::WriteOptions& options, rocksdb::WriteBatch* updates);

  rocksdb::Status Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key);

  rocksdb::Status Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  bool GetProperty(const rocksdb::Slice& property, std::string* value);

  rocksdb::Iterator* NewIterator(const rocksdb::ReadOptions& options);

  rocksdb::Status NewCheckpoint(rocksdb::Checkpoint** checkpoint);

  rocksdb::Status FlushWAL(bool sync);

  ~OpenRocksDB();

  rocksdb::DB* get();

 private:
  RocksDatabase& db_;
  rocksdb::DB* impl_;
};

class RocksDatabase {
  friend class OpenRocksDB;

 public:
  RocksDatabase(const rocksdb::Options& options, const std::string& name);

  utils::optional<OpenRocksDB> open();

  ~RocksDatabase();

 private:
  /*
   * notify RocksDatabase that the next open should check if they can reopen the database
   * until a successful reopen no more open is possible
   */
  void invalidate();

  rocksdb::Options open_options_;
  std::string db_name_;

  std::atomic<rocksdb::DB*> impl_{nullptr};
  std::atomic<int> reference_count_{0};

};

} /* namespace internal */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
