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

#include <utility>
#include <vector>
#include <string>
#include <memory>

#include "utils/file/FileUtils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ThreadedRepository.h"
#include "Connection.h"
#include "concurrentqueue.h"
#include "database/RocksDatabase.h"
#include "encryption/RocksDbEncryptionProvider.h"
#include "utils/crypto/EncryptionProvider.h"
#include "SwapManager.h"
#include "FlowFileLoader.h"
#include "range/v3/algorithm/all_of.hpp"
#include "utils/Literals.h"

namespace org::apache::nifi::minifi::core::repository {

#ifdef WIN32
constexpr auto FLOWFILE_REPOSITORY_DIRECTORY = ".\\flowfile_repository";
constexpr auto FLOWFILE_CHECKPOINT_DIRECTORY = ".\\flowfile_checkpoint";
#else
constexpr auto FLOWFILE_REPOSITORY_DIRECTORY = "./flowfile_repository";
constexpr auto FLOWFILE_CHECKPOINT_DIRECTORY = "./flowfile_checkpoint";
#endif
constexpr auto MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE = 10_MiB;
constexpr auto MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME = std::chrono::minutes(10);
constexpr auto FLOWFILE_REPOSITORY_PURGE_PERIOD = std::chrono::seconds(2);
constexpr auto FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS = std::chrono::milliseconds(500);

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using rocksdb as the primary substrate.
 */
class FlowFileRepository : public ThreadedRepository, public SwapManager {
 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.flowfile.repository.encryption.key";

  FlowFileRepository(std::string name, const utils::Identifier& /*uuid*/)
      : FlowFileRepository(std::move(name)) {
  }

  explicit FlowFileRepository(std::string repo_name = "",
                     std::string checkpoint_dir = FLOWFILE_CHECKPOINT_DIRECTORY,
                     std::string directory = FLOWFILE_REPOSITORY_DIRECTORY,
                     std::chrono::milliseconds maxPartitionMillis = MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME,
                     int64_t maxPartitionBytes = MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE,
                     std::chrono::milliseconds purgePeriod = FLOWFILE_REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        ThreadedRepository(repo_name.length() > 0 ? std::move(repo_name) : core::getClassName<FlowFileRepository>(), std::move(directory), maxPartitionMillis, maxPartitionBytes, purgePeriod),
        checkpoint_dir_(std::move(checkpoint_dir)),
        content_repo_(nullptr),
        checkpoint_(nullptr),
        logger_(logging::LoggerFactory<FlowFileRepository>::getLogger()) {
    db_ = nullptr;
  }

  ~FlowFileRepository() override {
    stop();
  }

  static auto properties() { return std::array<core::Property, 0>{}; }
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  bool isNoop() const override {
    return false;
  }

  void flush() override;

  virtual void printStats();

  bool initialize(const std::shared_ptr<Configure> &configure) override {
    config_ = configure;
    std::string value;

    if (configure->get(Configure::nifi_flowfile_repository_directory_default, value)) {
      directory_ = value;
    }
    logger_->log_debug("NiFi FlowFile Repository Directory %s", directory_);

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
    auto cf_options = [] (rocksdb::ColumnFamilyOptions& cf_opts) {
      cf_opts.OptimizeForPointLookup(4);
      cf_opts.write_buffer_size = 8ULL << 20U;
      cf_opts.max_write_buffer_number = 20;
      cf_opts.min_write_buffer_number_to_merge = 1;
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

  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override {
    // persistent to the DB
    auto opendb = db_->open();
    if (!opendb) {
      return false;
    }
    rocksdb::Slice value((const char *) buf, bufLen);
    auto operation = [&key, &value, &opendb]() { return opendb->Put(rocksdb::WriteOptions(), key, value); };
    return ExecuteWithRetry(operation);
  }

  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override {
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

  /**
   * Deletes the key
   * @return status of the delete operation
   */
  bool Delete(const std::string& key) override {
    keys_to_delete.enqueue(key);
    return true;
  }

  /**
   * Sets the value from the provided key
   * @return status of the get operation.
   */
  bool Get(const std::string &key, std::string &value) override {
    auto opendb = db_->open();
    if (!opendb) {
      return false;
    }
    return opendb->Get(rocksdb::ReadOptions(), key, &value).ok();
  }

  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) override;

  bool start() override {
    const bool ret = ThreadedRepository::start();
    if (swap_loader_) {
      swap_loader_->start();
    }
    return ret;
  }

  bool stop() override {
    if (swap_loader_) {
      swap_loader_->stop();
    }
    return ThreadedRepository::stop();
  }

  void store([[maybe_unused]] std::vector<std::shared_ptr<core::FlowFile>> flow_files) override {
    gsl_Expects(ranges::all_of(flow_files, &FlowFile::isStored));
    // pass, flowfiles are already persisted in the repository
  }

  std::future<std::vector<std::shared_ptr<core::FlowFile>>> load(std::vector<SwappedFlowFile> flow_files) override {
    return swap_loader_->load(std::move(flow_files));
  }

 private:
  void run() override;

  bool ExecuteWithRetry(const std::function<rocksdb::Status()>& operation);

  void initialize_repository();

  /**
   * Returns true if a checkpoint is needed at startup
   * @return true if a checkpoint is needed.
   */
  static bool need_checkpoint(minifi::internal::OpenRocksDb& opendb);

  void prune_stored_flowfiles();

  std::thread& getThread() override {
    return thread_;
  }

  std::string checkpoint_dir_;
  moodycamel::ConcurrentQueue<std::string> keys_to_delete;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  std::unique_ptr<rocksdb::Checkpoint> checkpoint_;
  std::unique_ptr<FlowFileLoader> swap_loader_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<minifi::Configure> config_;
  std::thread thread_;
};

}  // namespace org::apache::nifi::minifi::core::repository
