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
#pragma once

#include <cinttypes>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <utility>

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "core/Repository.h"
#include "core/Core.h"
#include "provenance/Provenance.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

#define PROVENANCE_DIRECTORY "./provenance_repository"
#define MAX_PROVENANCE_STORAGE_SIZE (10*1024*1024)  // 10M
constexpr auto MAX_PROVENANCE_ENTRY_LIFE_TIME = std::chrono::minutes(1);
constexpr auto PROVENANCE_PURGE_PERIOD = std::chrono::milliseconds(2500);

class ProvenanceRepository : public core::Repository, public std::enable_shared_from_this<ProvenanceRepository> {
 public:
  ProvenanceRepository(const std::string& name, const utils::Identifier& /*uuid*/)
      : ProvenanceRepository(name) {
  }
  // Constructor
  /*!
   * Create a new provenance repository
   */
  explicit ProvenanceRepository(const std::string& repo_name = "", std::string directory = PROVENANCE_DIRECTORY, std::chrono::milliseconds maxPartitionMillis = MAX_PROVENANCE_ENTRY_LIFE_TIME,
      int64_t maxPartitionBytes = MAX_PROVENANCE_STORAGE_SIZE, std::chrono::milliseconds purgePeriod = PROVENANCE_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        Repository(repo_name.length() > 0 ? repo_name : core::getClassName<ProvenanceRepository>(), directory, maxPartitionMillis, maxPartitionBytes, purgePeriod) {
    db_ = nullptr;
  }

  ~ProvenanceRepository() override {
    stop();
  }

  void printStats();

  bool isNoop() override {
    return false;
  }

  void start() override {
    if (running_)
      return;
    running_ = true;
    thread_ = std::thread(&ProvenanceRepository::run, shared_from_this());
    logger_->log_debug("%s Repository Monitor Thread Start", name_);
  }

  // initialize
  bool initialize(const std::shared_ptr<org::apache::nifi::minifi::Configure> &config) override {
    std::string value;
    if (config->get(Configure::nifi_provenance_repository_directory_default, value)) {
      directory_ = value;
    }
    logger_->log_debug("MiNiFi Provenance Repository Directory %s", directory_);
    if (config->get(Configure::nifi_provenance_repository_max_storage_size, value)) {
      core::Property::StringToInt(value, max_partition_bytes_);
    }
    logger_->log_debug("MiNiFi Provenance Max Partition Bytes %d", max_partition_bytes_);
    if (config->get(Configure::nifi_provenance_repository_max_storage_time, value)) {
      if (auto max_partition = utils::timeutils::StringToDuration<std::chrono::milliseconds>(value))
          max_partition_millis_ = *max_partition;
    }
    logger_->log_debug("MiNiFi Provenance Max Storage Time: [%" PRId64 "] ms", int64_t{max_partition_millis_.count()});
    rocksdb::Options options;
    options.create_if_missing = true;
    options.use_direct_io_for_flush_and_compaction = true;
    options.use_direct_reads = true;
    // Rocksdb write buffers act as a log of database operation: grow till reaching the limit, serialized after
    // This shouldn't go above 16MB and the configured total size of the db should cap it as well
    int64_t max_buffer_size = 16 << 20;
    options.write_buffer_size = gsl::narrow<size_t>(std::min(max_buffer_size, max_partition_bytes_));
    options.max_write_buffer_number = 4;
    options.min_write_buffer_number_to_merge = 1;

    options.compaction_style = rocksdb::CompactionStyle::kCompactionStyleFIFO;
    options.compaction_options_fifo = rocksdb::CompactionOptionsFIFO(max_partition_bytes_, false);
    if (max_partition_millis_ > std::chrono::milliseconds(0)) {
      options.ttl = std::chrono::duration_cast<std::chrono::seconds>(max_partition_millis_).count();
    }

    logger_->log_info("Write buffer: %llu", options.write_buffer_size);
    logger_->log_info("Max partition bytes: %llu", max_partition_bytes_);
    logger_->log_info("Ttl: %llu", options.ttl);

    rocksdb::DB* db;
    rocksdb::Status status = rocksdb::DB::Open(options, directory_, &db);
    if (status.ok()) {
      logger_->log_debug("MiNiFi Provenance Repository database open %s success", directory_);
      db_.reset(db);
    } else {
      logger_->log_error("MiNiFi Provenance Repository database open %s failed: %s", directory_, status.ToString());
      return false;
    }

    return true;
  }
  // Put
  bool Put(std::string key, const uint8_t *buf, size_t bufLen) override {
    // persist to the DB
    rocksdb::Slice value((const char *) buf, bufLen);
    return db_->Put(rocksdb::WriteOptions(), key, value).ok();
  }

  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override {
    rocksdb::WriteBatch batch;
    for (const auto &item : data) {
      const auto buf = item.second->getBuffer().as_span<const char>();
      rocksdb::Slice value(buf.data(), buf.size());
      if (!batch.Put(item.first, value).ok()) {
        return false;
      }
    }
    return db_->Write(rocksdb::WriteOptions(), &batch).ok();
  }

  // Delete
  bool Delete(std::string /*key*/) override {
    // The repo is cleaned up by itself, there is no need to delete items.
    return true;
  }
  // Get
  bool Get(const std::string &key, std::string &value) override {
    return db_->Get(rocksdb::ReadOptions(), key, &value).ok();
  }

  bool Serialize(const std::string &key, const uint8_t *buffer, const size_t bufferSize) override {
    return Put(key, buffer, bufferSize);
  }

  virtual bool get(std::vector<std::shared_ptr<core::CoreComponent>> &store, size_t max_size) {
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
      std::string key = it->key().ToString();
      if (store.size() >= max_size)
        break;
      if (eventRead->DeSerialize(gsl::make_span(it->value()).as_span<const std::byte>())) {
        store.push_back(std::dynamic_pointer_cast<core::CoreComponent>(eventRead));
      }
    }
    return true;
  }

  bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &records, size_t &max_size, std::function<std::shared_ptr<core::SerializableComponent>()> lambda) override {
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));
    size_t requested_batch = max_size;
    max_size = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (max_size >= requested_batch)
        break;
      std::shared_ptr<core::SerializableComponent> eventRead = lambda();
      std::string key = it->key().ToString();
      if (eventRead->DeSerialize(gsl::make_span(it->value()).as_span<const std::byte>())) {
        max_size++;
        records.push_back(eventRead);
      }
    }
    return max_size > 0;
  }

  //! get record
  void getProvenanceRecord(std::vector<std::shared_ptr<ProvenanceEventRecord>> &records, int maxSize) {
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
      std::string key = it->key().ToString();
      if (records.size() >= (uint64_t)maxSize)
        break;
      if (eventRead->DeSerialize(gsl::make_span(it->value()).as_span<const std::byte>())) {
        records.push_back(eventRead);
      }
    }
  }

  bool DeSerialize(std::vector<std::shared_ptr<core::SerializableComponent>> &store, size_t &max_size) override {
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(rocksdb::ReadOptions()));
    max_size = 0;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::shared_ptr<ProvenanceEventRecord> eventRead = std::make_shared<ProvenanceEventRecord>();
      std::string key = it->key().ToString();

      if (store.at(max_size)->DeSerialize(gsl::make_span(it->value()).as_span<const std::byte>())) {
        max_size++;
      }
      if (store.size() >= max_size)
        break;
    }
    return max_size > 0;
  }

  // destroy
  void destroy() {
    db_.reset();
  }
  // Run function for the thread
  void run() override;

  uint64_t getKeyCount() const {
    std::string key_count;
    db_->GetProperty("rocksdb.estimate-num-keys", &key_count);

    return std::stoull(key_count);
  }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceRepository(const ProvenanceRepository &parent) = delete;
  ProvenanceRepository &operator=(const ProvenanceRepository &parent) = delete;

 private:
  std::unique_ptr<rocksdb::DB> db_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ProvenanceRepository>::getLogger();
};

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
