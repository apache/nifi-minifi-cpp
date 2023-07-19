/**
 * @file Repository
 * Repository class declaration
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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ResourceClaim.h"
#include "core/Connectable.h"
#include "core/ContentRepository.h"
#include "core/Property.h"
#include "core/SerializableComponent.h"
#include "core/logging/LoggerFactory.h"
#include "core/RepositoryMetricsSource.h"
#include "properties/Configure.h"
#include "utils/BackTrace.h"
#include "SwapManager.h"
#include "utils/Literals.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtil.h"
#include "core/Core.h"

#ifndef WIN32
#include <sys/stat.h>
#endif

namespace org::apache::nifi::minifi::core {

constexpr auto REPOSITORY_DIRECTORY = "./repo";
constexpr auto MAX_REPOSITORY_STORAGE_SIZE = 10_MiB;
constexpr auto MAX_REPOSITORY_ENTRY_LIFE_TIME = std::chrono::minutes(10);
constexpr auto REPOSITORY_PURGE_PERIOD = std::chrono::milliseconds(2500);

class Repository : public core::CoreComponent, public core::RepositoryMetricsSource {
 public:
  explicit Repository(std::string_view repo_name = "Repository",
                      std::string directory = REPOSITORY_DIRECTORY,
                      std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
                      int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
                      std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
    : core::CoreComponent(repo_name),
      max_partition_millis_(maxPartitionMillis),
      max_partition_bytes_(maxPartitionBytes),
      purge_period_(purgePeriod),
      repo_full_(false),
      directory_(std::move(directory)),
      logger_(logging::LoggerFactory<Repository>::getLogger()) {
  }

  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;
  virtual bool start() = 0;
  virtual bool stop() = 0;

  virtual bool isNoop() const {
    return true;
  }

  virtual void flush() {
  }

  virtual bool Put(const std::string& /*key*/, const uint8_t* /*buf*/, size_t /*bufLen*/) {
    return true;
  }

  virtual bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>>& /*data*/) {
    return true;
  }

  virtual bool Delete(const std::string& /*key*/) {
    return true;
  }

  virtual bool Delete(const std::shared_ptr<core::CoreComponent>& item) {
    return Delete(item->getUUIDStr());
  }

  virtual bool Delete(std::vector<std::shared_ptr<core::SerializableComponent>> &storedValues);

  void setConnectionMap(std::map<std::string, core::Connectable*> connectionMap) {
    connection_map_ = std::move(connectionMap);
  }

  void setContainers(std::map<std::string, core::Connectable*> containers) {
    containers_ = std::move(containers);
  }

  virtual bool Get(const std::string& /*key*/, std::string& /*value*/) {
    return false;
  }

  virtual bool getElements(std::vector<std::shared_ptr<core::SerializableComponent>>& /*store*/, size_t& /*max_size*/) {
    return true;
  }

  virtual bool storeElement(const std::shared_ptr<core::SerializableComponent>& element);

  virtual void loadComponent(const std::shared_ptr<core::ContentRepository>& /*content_repo*/) {
  }

  std::string getDirectory() const {
    return directory_;
  }

  std::string getRepositoryName() const override {
    return getName();
  }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Repository(const Repository &parent) = delete;
  Repository &operator=(const Repository &parent) = delete;

 protected:
  std::map<std::string, core::Connectable*> containers_;

  std::map<std::string, core::Connectable*> connection_map_;
  // max db entry lifetime
  std::chrono::milliseconds max_partition_millis_;
  // max db size
  int64_t max_partition_bytes_;
  std::chrono::milliseconds purge_period_;
  std::atomic<bool> repo_full_;
  std::string directory_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core
