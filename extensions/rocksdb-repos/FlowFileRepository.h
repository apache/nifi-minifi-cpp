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

#include <utility>
#include <vector>
#include <string>
#include <memory>

#include "utils/file/FileUtils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"
#include "core/Repository.h"
#include "core/Core.h"
#include "Connection.h"
#include "core/logging/LoggerConfiguration.h"
#include "concurrentqueue.h"
#include "database/RocksDatabase.h"
#include "encryption/RocksDbEncryptionProvider.h"
#include "utils/crypto/EncryptionProvider.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

#ifdef WIN32
#define FLOWFILE_REPOSITORY_DIRECTORY ".\\flowfile_repository"
#define FLOWFILE_CHECKPOINT_DIRECTORY ".\\flowfile_checkpoint"
#else
#define FLOWFILE_REPOSITORY_DIRECTORY "./flowfile_repository"
#define FLOWFILE_CHECKPOINT_DIRECTORY "./flowfile_checkpoint"
#endif
#define MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE (10*1024*1024)  // 10M
#define MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME (600000)  // 10 minute
#define FLOWFILE_REPOSITORY_PURGE_PERIOD (2000)  // 2000 msec
#define FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS (500)  // msec

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using rocksdb as the primary substrate.
 */
class FlowFileRepository : public core::Repository, public std::enable_shared_from_this<FlowFileRepository> {
 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.flowfile.repository.encryption.key";
  // Constructor

  FlowFileRepository(const std::string& name, const utils::Identifier& /*uuid*/)
      : FlowFileRepository(name) {
  }

  FlowFileRepository(const std::string repo_name = "", const std::string& checkpoint_dir = FLOWFILE_CHECKPOINT_DIRECTORY,
                     std::string directory = FLOWFILE_REPOSITORY_DIRECTORY, int64_t maxPartitionMillis = MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME,
                     int64_t maxPartitionBytes = MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE, uint64_t purgePeriod = FLOWFILE_REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        Repository(repo_name.length() > 0 ? repo_name : core::getClassName<FlowFileRepository>(), directory, maxPartitionMillis, maxPartitionBytes, purgePeriod),
        checkpoint_dir_(checkpoint_dir),
        content_repo_(nullptr),
        checkpoint_(nullptr),
        logger_(logging::LoggerFactory<FlowFileRepository>::getLogger()) {
    db_ = NULL;
  }

  virtual bool isNoop() {
    return false;
  }

  virtual void flush();

  virtual void printStats();

  // initialize
  virtual bool initialize(const std::shared_ptr<Configure> &configure) {
    config_ = configure;
    std::string value;

    if (configure->get(Configure::nifi_flowfile_repository_directory_default, value)) {
      directory_ = value;
    }
    logger_->log_debug("NiFi FlowFile Repository Directory %s", directory_);
    if (configure->get(Configure::nifi_flowfile_repository_max_storage_size, value)) {
      Property::StringToInt(value, max_partition_bytes_);
    }
    logger_->log_debug("NiFi FlowFile Max Partition Bytes %d", max_partition_bytes_);
    if (configure->get(Configure::nifi_flowfile_repository_max_storage_time, value)) {
      TimeUnit unit;
      if (Property::StringToTime(value, max_partition_millis_, unit)) {
        Property::ConvertTimeUnitToMS(max_partition_millis_, unit, max_partition_millis_);
      }
    }
    logger_->log_debug("NiFi FlowFile Max Storage Time: [%d] ms", max_partition_millis_);

    const auto encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{configure->getHome()}, DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME});
    logger_->log_info("Using %s FlowFileRepository", encrypted_env ? "encrypted" : "plaintext");

    auto db_options = [encrypted_env] (minifi::internal::Writable<rocksdb::DBOptions>& options) {
      options.set(&rocksdb::DBOptions::create_if_missing, true);
      options.set(&rocksdb::DBOptions::use_direct_io_for_flush_and_compaction, true);
      options.set(&rocksdb::DBOptions::use_direct_reads, true);
      if (encrypted_env) {
        options.set(&rocksdb::DBOptions::env, encrypted_env.get(), EncryptionEq{});
      } else {
        options.set(&rocksdb::DBOptions::env, rocksdb::Env::Default());
      }
    };

    // Write buffers are used as db operation logs. When they get filled the events are merged and serialized.
    // The default size is 64MB.
    // In our case it's usually too much, causing sawtooth in memory consumption. (Consumes more than the whole MiniFi)
    // To avoid DB write issues during heavy load it's recommended to have high number of buffer.
    // Rocksdb's stall feature can also trigger in case the number of buffers is >= 3.
    // The more buffers we have the more memory rocksdb can utilize without significant memory consumption under low load.
    auto cf_options = [] (minifi::internal::Writable<rocksdb::ColumnFamilyOptions>& cf_opts) {
      cf_opts.set(&rocksdb::ColumnFamilyOptions::write_buffer_size, 8ULL << 20U);
      cf_opts.set<int>(&rocksdb::ColumnFamilyOptions::max_write_buffer_number, 20);
      cf_opts.set<int>(&rocksdb::ColumnFamilyOptions::min_write_buffer_number_to_merge, 1);
    };
    db_ = minifi::internal::RocksDatabase::create(db_options, cf_options, directory_);
    if (db_->open()) {
      logger_->log_debug("NiFi FlowFile Repository database open %s success", directory_);
      return true;
    } else {
      logger_->log_error("NiFi FlowFile Repository database open %s fail", directory_);
      return false;
    }
  }

  virtual void run();

  virtual bool Put(std::string key, const uint8_t *buf, size_t bufLen) {
    // persistent to the DB
    auto opendb = db_->open();
    if (!opendb) {
      return false;
    }
    rocksdb::Slice value((const char *) buf, bufLen);
    auto operation = [&key, &value, &opendb]() { return opendb->Put(rocksdb::WriteOptions(), key, value); };
    return ExecuteWithRetry(operation);
  }

  virtual bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) {
    auto opendb = db_->open();
    if (!opendb) {
      return false;
    }
    auto batch = opendb->createWriteBatch();
    for (const auto &item : data) {
      rocksdb::Slice value((const char *) item.second->getBuffer(), item.second->size());
      if (!batch.Put(item.first, value).ok()) {
        logger_->log_error("Failed to add item to batch operation");
        return false;
      }
    }
    auto operation = [&batch, &opendb]() { return opendb->Write(rocksdb::WriteOptions(), &batch); };
    return ExecuteWithRetry(operation);
  }


  /**
   *
   * Deletes the key
   * @return status of the delete operation
   */
  virtual bool Delete(std::string key) {
    keys_to_delete.enqueue(key);
    return true;
  }
  /**
   * Sets the value from the provided key
   * @return status of the get operation.
   */
  virtual bool Get(const std::string &key, std::string &value) {
    auto opendb = db_->open();
    if (!opendb) {
      return false;
    }
    return opendb->Get(rocksdb::ReadOptions(), key, &value).ok();
  }

  virtual void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo);

  void start() {
    if (this->purge_period_ <= 0) {
      return;
    }
    if (running_) {
      return;
    }
    running_ = true;
    thread_ = std::thread(&FlowFileRepository::run, shared_from_this());
    logger_->log_debug("%s Repository Monitor Thread Start", getName());
  }

 private:
  bool ExecuteWithRetry(std::function<rocksdb::Status()> operation);

  /**
   * Initialize the repository
   */
  void initialize_repository();

  /**
   * Returns true if a checkpoint is needed at startup
   * @return true if a checkpoint is needed.
   */
  bool need_checkpoint(minifi::internal::OpenRocksDb& opendb);

  /**
   * Prunes stored flow files.
   */
  void prune_stored_flowfiles();

  std::string checkpoint_dir_;
  moodycamel::ConcurrentQueue<std::string> keys_to_delete;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  std::unique_ptr<rocksdb::Checkpoint> checkpoint_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<minifi::Configure> config_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
