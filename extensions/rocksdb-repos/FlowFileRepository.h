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
#include <string_view>
#include <memory>
#include <list>

#include "utils/file/FileUtils.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/Connection.h"
#include "concurrentqueue.h"
#include "database/RocksDatabase.h"
#include "encryption/RocksDbEncryptionProvider.h"
#include "utils/crypto/EncryptionProvider.h"
#include "minifi-cpp/SwapManager.h"
#include "FlowFileLoader.h"
#include "range/v3/algorithm/all_of.hpp"
#include "utils/Literals.h"
#include "utils/StoppableThread.h"
#include "RocksDbRepository.h"

namespace org::apache::nifi::minifi {
class FlowFileRecord;
}
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

/**
 * Flow File repository
 * Design: Extends Repository and implements the run function, using rocksdb as the primary substrate.
 */
class FlowFileRepository : public RocksDbRepository, public SwapManager {
  static constexpr std::chrono::milliseconds DEFAULT_COMPACTION_PERIOD = std::chrono::minutes{2};

  struct ExpiredFlowFileInfo {
    std::string key;
    std::shared_ptr<ResourceClaim> content{};
  };

 public:
  static constexpr const char* ENCRYPTION_KEY_NAME = "nifi.flowfile.repository.encryption.key";

  FlowFileRepository(std::string_view name, const utils::Identifier& /*uuid*/)
    : FlowFileRepository(name) {
  }

  explicit FlowFileRepository(const std::string_view repo_name = "",
                              std::string directory = FLOWFILE_REPOSITORY_DIRECTORY,
                              std::chrono::milliseconds maxPartitionMillis = MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME,
                              int64_t maxPartitionBytes = MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE,
                              std::chrono::milliseconds purgePeriod = FLOWFILE_REPOSITORY_PURGE_PERIOD)
    : RocksDbRepository(repo_name.length() > 0 ? repo_name : core::className<FlowFileRepository>(),
                        std::move(directory), maxPartitionMillis, maxPartitionBytes, purgePeriod, logging::LoggerFactory<FlowFileRepository>::getLogger()) {
  }

  ~FlowFileRepository() override {
    stop();
  }

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  bool isNoop() const override {
    return false;
  }

  bool Delete(const std::string& key) override;
  bool Delete(const std::shared_ptr<core::CoreComponent>& item) override;

  void flush() override;
  bool initialize(const std::shared_ptr<Configure> &configure) override;
  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) override;
  bool start() override;
  bool stop() override;
  void store([[maybe_unused]] std::vector<std::shared_ptr<core::FlowFile>> flow_files) override;
  std::future<std::vector<std::shared_ptr<core::FlowFile>>> load(std::vector<SwappedFlowFile> flow_files) override;

 private:
  void run() override;
  void initialize_repository();

  void runCompaction();
  void setCompactionPeriod(const std::shared_ptr<Configure> &configure);

  void deserializeFlowFilesWithNoContentClaim(minifi::internal::OpenRocksDb& opendb, std::list<ExpiredFlowFileInfo>& flow_files);

  bool contentSizeIsAmpleForFlowFile(const FlowFile& flow_file_record, const std::shared_ptr<ResourceClaim>& resource_claim) const;
  Connectable* getContainer(const std::string& container_id);

  moodycamel::ConcurrentQueue<ExpiredFlowFileInfo> keys_to_delete_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::unique_ptr<FlowFileLoader> swap_loader_;
  std::shared_ptr<minifi::Configure> config_;

  std::chrono::milliseconds compaction_period_;
  std::unique_ptr<utils::StoppableThread> compaction_thread_;
  bool check_flowfile_content_size_ = true;
};

}  // namespace org::apache::nifi::minifi::core::repository
