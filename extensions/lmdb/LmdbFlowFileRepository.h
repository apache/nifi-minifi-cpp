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

#include "core/ThreadedRepository.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/Connection.h"
#include "concurrentqueue.h"
#include "minifi-cpp/utils/Literals.h"
#include "LmdbWrapper.h"

namespace org::apache::nifi::minifi {
class FlowFileRecord;
}
namespace org::apache::nifi::minifi::extensions::lmdb {

#ifdef WIN32
constexpr auto FLOWFILE_REPOSITORY_DIRECTORY = ".\\flowfile_repository";
constexpr auto FLOWFILE_CHECKPOINT_DIRECTORY = ".\\flowfile_checkpoint";
#else
constexpr auto FLOWFILE_REPOSITORY_DIRECTORY = "./flowfile_repository";
constexpr auto FLOWFILE_CHECKPOINT_DIRECTORY = "./flowfile_checkpoint";
#endif
constexpr auto MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE = 100_MiB;
constexpr auto MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME = std::chrono::minutes(10);
constexpr auto FLOWFILE_REPOSITORY_PURGE_PERIOD = std::chrono::seconds(2);

constexpr auto FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS = std::chrono::milliseconds(500);

class LmdbFlowFileRepository : public core::ThreadedRepositoryImpl {
 public:
  LmdbFlowFileRepository(std::string_view name, const utils::Identifier& /*uuid*/)
    : LmdbFlowFileRepository(name) {
  }

  explicit LmdbFlowFileRepository(const std::string_view repo_name = "",
                                  std::string directory = FLOWFILE_REPOSITORY_DIRECTORY,
                                  int64_t max_partition_bytes = MAX_FLOWFILE_REPOSITORY_STORAGE_SIZE,
                                  std::chrono::milliseconds purge_period = FLOWFILE_REPOSITORY_PURGE_PERIOD)
      : ThreadedRepositoryImpl(repo_name.length() > 0 ? repo_name : core::className<LmdbFlowFileRepository>(), std::move(directory),
                               MAX_FLOWFILE_REPOSITORY_ENTRY_LIFE_TIME, max_partition_bytes, purge_period) {
  }

  ~LmdbFlowFileRepository() override {
    stop();
  }

  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override;
  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) override;
  bool Get(const std::string &key, std::string &value) override;
  bool Delete(const std::string& key) override;
  bool Delete(const std::shared_ptr<core::CoreComponent>& item) override;

  bool isNoop() const override {
    return false;
  }

  void flush() override;
  bool initialize(const std::shared_ptr<Configure> &configure) override;
  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) override;

  uint64_t getRepositorySize() const override;
  uint64_t getRepositoryEntryCount() const override;

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

 private:
  std::thread& getThread() override {
    return thread_;
  }

  struct ExpiredFlowFileInfo {
    std::string key;
    std::shared_ptr<ResourceClaim> content{};
  };

  void run() override;
  void initialize_repository();

  void deserializeFlowFilesWithNoContentClaim(std::list<ExpiredFlowFileInfo>& flow_files);

  bool contentSizeIsAmpleForFlowFile(const core::FlowFile& flow_file_record, const std::shared_ptr<ResourceClaim>& resource_claim) const;
  core::Connectable* getContainer(const std::string& container_id);

  moodycamel::ConcurrentQueue<ExpiredFlowFileInfo> keys_to_delete_;
  std::shared_ptr<core::ContentRepository> content_repo_;

  bool check_flowfile_content_size_ = true;

  std::thread thread_;
  LmdbWrapper lmdb_wrapper_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<LmdbFlowFileRepository>::getLogger();
};

}  // namespace org::apache::nifi::minifi::extensions::lmdb
