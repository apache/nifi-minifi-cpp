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
#include "utils/StoppableThread.h"

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
  static constexpr std::chrono::milliseconds DEFAULT_COMPACTION_PERIOD = std::chrono::seconds{120};

 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.flowfile.repository.encryption.key";

  FlowFileRepository(std::string name, const utils::Identifier& /*uuid*/)
    : FlowFileRepository(std::move(name)) {
  }

  explicit FlowFileRepository(const std::string& repo_name = "",
                              std::string directory = FLOWFILE_REPOSITORY_DIRECTORY,
                              std::chrono::milliseconds maxPartitionMillis = MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME,
                              int64_t maxPartitionBytes = MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE,
                              std::chrono::milliseconds purgePeriod = FLOWFILE_REPOSITORY_PURGE_PERIOD)
    : ThreadedRepository(repo_name.length() > 0 ? std::move(repo_name) : core::getClassName<FlowFileRepository>(), std::move(directory), maxPartitionMillis, maxPartitionBytes, purgePeriod),
      logger_(logging::LoggerFactory<FlowFileRepository>::getLogger()) {
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

  bool initialize(const std::shared_ptr<Configure> &configure) override;

  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override;
  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override;
  bool Delete(const std::string& key) override;
  bool Get(const std::string &key, std::string &value) override;

  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) override;
  bool start() override;
  bool stop() override;
  void store([[maybe_unused]] std::vector<std::shared_ptr<core::FlowFile>> flow_files) override;
  std::future<std::vector<std::shared_ptr<core::FlowFile>>> load(std::vector<SwappedFlowFile> flow_files) override;

 private:
  void run() override;

  void runCompaction();
  void setCompactionPeriod(const std::shared_ptr<Configure> &configure);

  bool ExecuteWithRetry(const std::function<rocksdb::Status()>& operation);

  void initialize_repository();

  std::thread& getThread() override {
    return thread_;
  }

  moodycamel::ConcurrentQueue<std::string> keys_to_delete;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::unique_ptr<minifi::internal::RocksDatabase> db_;
  std::unique_ptr<FlowFileLoader> swap_loader_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<minifi::Configure> config_;
  std::thread thread_;

  std::chrono::milliseconds compaction_period_;
  std::unique_ptr<utils::StoppableThread> compaction_thread_;
};

}  // namespace org::apache::nifi::minifi::core::repository
