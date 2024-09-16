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

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include "utils/gsl.h"

#include "rocksdb/db.h"
#include "rocksdb/utilities/checkpoint.h"
#include "WriteBatch.h"
#include "core/RepositoryMetricsSource.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::internal {

class RocksDbInstance;
struct ColumnHandle;

class OpenRocksDb {
  friend class RocksDbInstance;

  OpenRocksDb(RocksDbInstance& db, gsl::not_null<std::shared_ptr<rocksdb::DB>> impl, gsl::not_null<std::shared_ptr<ColumnHandle>> column);

 public:
  OpenRocksDb(const OpenRocksDb&) = delete;
  OpenRocksDb(OpenRocksDb&&) noexcept = default;
  OpenRocksDb& operator=(const OpenRocksDb&) = delete;
  OpenRocksDb& operator=(OpenRocksDb&&) = default;

  WriteBatch createWriteBatch() const noexcept;

  rocksdb::Status Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  rocksdb::Status Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value);

  std::vector<rocksdb::Status> MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values);

  rocksdb::Status Write(const rocksdb::WriteOptions& options, internal::WriteBatch* updates);

  rocksdb::Status Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key);

  rocksdb::Status Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  bool GetProperty(const rocksdb::Slice& property, std::string* value);

  std::unique_ptr<rocksdb::Iterator> NewIterator(const rocksdb::ReadOptions& options);

  rocksdb::Status NewCheckpoint(rocksdb::Checkpoint** checkpoint);

  rocksdb::Status NewCheckpoint(std::unique_ptr<rocksdb::Checkpoint>& checkpoint);

  rocksdb::Status FlushWAL(bool sync);

  rocksdb::Status RunCompaction();

  rocksdb::DB* get();

  std::optional<uint64_t> getApproximateSizes() const;
  minifi::core::RepositoryMetricsSource::RocksDbStats getStats();

 private:
  void handleResult(const rocksdb::Status& result);
  void handleResult(const std::vector<rocksdb::Status>& results);

  gsl::not_null<RocksDbInstance*> db_;
  gsl::not_null<std::shared_ptr<rocksdb::DB>> impl_;
  gsl::not_null<std::shared_ptr<ColumnHandle>> column_;
  std::shared_ptr<minifi::core::logging::Logger> logger_{minifi::core::logging::LoggerFactory<OpenRocksDb>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::internal
