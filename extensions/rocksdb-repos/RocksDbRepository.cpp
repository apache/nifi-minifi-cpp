/**
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
#include "RocksDbRepository.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::repository {

void RocksDbRepository::printStats() {
  auto opendb = db_->open();
  if (!opendb) {
    return;
  }
  std::string key_count;
  opendb->GetProperty("rocksdb.estimate-num-keys", &key_count);

  std::string table_readers;
  opendb->GetProperty("rocksdb.estimate-table-readers-mem", &table_readers);

  std::string all_memtables;
  opendb->GetProperty("rocksdb.cur-size-all-mem-tables", &all_memtables);

  logger_->log_info("Repository stats: key count: %s, table readers size: %s, all memory tables size: %s",
      key_count, table_readers, all_memtables);
}

bool RocksDbRepository::ExecuteWithRetry(const std::function<rocksdb::Status()>& operation) {
  constexpr int RETRY_COUNT = 3;
  std::chrono::milliseconds wait_time = 0ms;
  for (int i=0; i < RETRY_COUNT; ++i) {
    auto status = operation();
    if (status.ok()) {
      logger_->log_trace("Rocksdb operation executed successfully");
      return true;
    }
    logger_->log_error("Rocksdb operation failed: %s", status.ToString());
    wait_time += FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS;
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

bool RocksDbRepository::Put(const std::string& key, const uint8_t *buf, size_t bufLen) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Slice value((const char *) buf, bufLen);
  auto operation = [&key, &value, &opendb]() { return opendb->Put(rocksdb::WriteOptions(), key, value); };
  return ExecuteWithRetry(operation);
}

bool RocksDbRepository::MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  auto batch = opendb->createWriteBatch();
  for (const auto &item : data) {
    const auto buf = item.second->getBuffer().as_span<const char>();
    rocksdb::Slice value(buf.data(), buf.size());
    if (!batch.Put(item.first, value).ok()) {
      logger_->log_error("Failed to add item to batch operation");
      return false;
    }
  }
  auto operation = [&batch, &opendb]() { return opendb->Write(rocksdb::WriteOptions(), &batch); };
  return ExecuteWithRetry(operation);
}

bool RocksDbRepository::Get(const std::string &key, std::string &value) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  return opendb->Get(rocksdb::ReadOptions(), key, &value).ok();
}

uint64_t RocksDbRepository::getRepositorySize() const {
  return (utils::optional_from_ptr(db_.get()) |
          utils::flatMap([](const auto& db) { return db->open(); }) |
          utils::flatMap([](const auto& opendb) { return opendb.getApproximateSizes(); })).value_or(0);
}

uint64_t RocksDbRepository::getRepositoryEntryCount() const {
  return (utils::optional_from_ptr(db_.get()) |
          utils::flatMap([](const auto& db) { return db->open(); }) |
          utils::flatMap([](auto&& opendb) -> std::optional<uint64_t> {
              std::string key_count;
              opendb.GetProperty("rocksdb.estimate-num-keys", &key_count);
              if (!key_count.empty()) {
                return std::stoull(key_count);
              }
              return std::nullopt;
            })).value_or(0);
}

}  // namespace org::apache::nifi::minifi::core::repository
