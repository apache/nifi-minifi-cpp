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

#include <utility>

#include "OpenRocksDb.h"
#include "ColumnHandle.h"
#include "RocksDbInstance.h"

namespace org::apache::nifi::minifi::internal {

OpenRocksDb::OpenRocksDb(RocksDbInstance& db, gsl::not_null<std::shared_ptr<rocksdb::DB>> impl, gsl::not_null<std::shared_ptr<ColumnHandle>> column)
    : db_(&db), impl_(std::move(impl)), column_(std::move(column)) {}

rocksdb::Status OpenRocksDb::Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Put(options, column_->handle.get(), key, value);
  handleResult(result);
  return result;
}

rocksdb::Status OpenRocksDb::Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value) {
  rocksdb::Status result = impl_->Get(options, column_->handle.get(), key, value);
  handleResult(result);
  return result;
}

std::vector<rocksdb::Status> OpenRocksDb::MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values) {
  std::vector<rocksdb::Status> results = impl_->MultiGet(
      options, std::vector<rocksdb::ColumnFamilyHandle*>(keys.size(), column_->handle.get()), keys, values);
  handleResult(results);
  return results;
}

rocksdb::Status OpenRocksDb::Write(const rocksdb::WriteOptions& options, internal::WriteBatch* updates) {
  rocksdb::Status result = impl_->Write(options, &updates->impl_);
  handleResult(result);
  return result;
}

rocksdb::Status OpenRocksDb::Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key) {
  rocksdb::Status result = impl_->Delete(options, column_->handle.get(), key);
  handleResult(result);
  return result;
}

rocksdb::Status OpenRocksDb::Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Merge(options, column_->handle.get(), key, value);
  handleResult(result);
  return result;
}

bool OpenRocksDb::GetProperty(const rocksdb::Slice& property, std::string* value) {
  return impl_->GetProperty(column_->handle.get(), property, value);
}

std::unique_ptr<rocksdb::Iterator> OpenRocksDb::NewIterator(const rocksdb::ReadOptions& options) {
  return std::unique_ptr<rocksdb::Iterator>{impl_->NewIterator(options, column_->handle.get())};
}

rocksdb::Status OpenRocksDb::NewCheckpoint(rocksdb::Checkpoint **checkpoint) {
  return rocksdb::Checkpoint::Create(impl_.get(), checkpoint);
}

rocksdb::Status OpenRocksDb::NewCheckpoint(std::unique_ptr<rocksdb::Checkpoint>& checkpoint) {
  rocksdb::Checkpoint* checkpoint_ptr = nullptr;
  rocksdb::Status result = NewCheckpoint(&checkpoint_ptr);
  if (result.ok()) {
    checkpoint.reset(checkpoint_ptr);
  }
  return result;
}

rocksdb::Status OpenRocksDb::FlushWAL(bool sync) {
  rocksdb::Status result = impl_->FlushWAL(sync);
  handleResult(result);
  return result;
}

rocksdb::Status OpenRocksDb::RunCompaction() {
  rocksdb::Status result = impl_->CompactRange(rocksdb::CompactRangeOptions{
    .bottommost_level_compaction = rocksdb::BottommostLevelCompaction::kForce
  }, nullptr, nullptr);
  handleResult(result);
  return result;
}

void OpenRocksDb::handleResult(const rocksdb::Status& result) {
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
}

void OpenRocksDb::handleResult(const std::vector<rocksdb::Status>& results) {
  for (const auto& result : results) {
    if (result == rocksdb::Status::NoSpace()) {
      db_->invalidate();
      break;
    }
  }
}

WriteBatch OpenRocksDb::createWriteBatch() const noexcept {
  return WriteBatch(column_->handle.get());
}

rocksdb::DB* OpenRocksDb::get() {
  return impl_.get();
}

std::optional<uint64_t> OpenRocksDb::getApproximateSizes() const {
  const rocksdb::SizeApproximationOptions options{ .include_memtables = true };
  std::string del_char_str(1, static_cast<char>(127));
  std::string empty_str;
  const rocksdb::Range range(empty_str, del_char_str);
  uint64_t value = 0;
  auto status = impl_->GetApproximateSizes(options, column_->handle.get(), &range, 1, &value);
  if (status.ok()) {
    return value;
  }
  return std::nullopt;
}

minifi::core::RepositoryMetricsSource::RocksDbStats OpenRocksDb::getStats() {
  minifi::core::RepositoryMetricsSource::RocksDbStats stats;
  {
    std::string table_readers;
    GetProperty("rocksdb.estimate-table-readers-mem", &table_readers);
    try {
      stats.table_readers_size = std::stoull(table_readers);
    } catch (const std::exception&) {
      logger_->log_warn("Could not retrieve valid 'rocksdb.estimate-table-readers-mem' property value from rocksdb content repository!");
    }
  }

  {
    std::string all_memtables;
    GetProperty("rocksdb.cur-size-all-mem-tables", &all_memtables);
    try {
      stats.all_memory_tables_size = std::stoull(all_memtables);
    } catch (const std::exception&) {
      logger_->log_warn("Could not retrieve valid 'rocksdb.cur-size-all-mem-tables' property value from rocksdb content repository!");
    }
  }

  {
    std::string block_cache_usage;
    GetProperty("rocksdb.block-cache-usage", &block_cache_usage);
    try {
      stats.block_cache_usage = std::stoull(block_cache_usage);
    } catch (const std::exception&) {
      logger_->log_warn("Could not retrieve valid 'rocksdb.block-cache-usage' property value from rocksdb content repository!");
    }
  }

  {
    std::string block_cache_pinned_usage;
    GetProperty("rocksdb.block-cache-pinned-usage", &block_cache_pinned_usage);
    try {
      stats.block_cache_pinned_usage = std::stoull(block_cache_pinned_usage);
    } catch (const std::exception&) {
      logger_->log_warn("Could not retrieve valid 'rocksdb.block-cache-pinned-usage' property value from rocksdb content repository!");
    }
  }

  return stats;
}

}  // namespace org::apache::nifi::minifi::internal
