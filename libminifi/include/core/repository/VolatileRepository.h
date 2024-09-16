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

#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cinttypes>

#include "AtomicRepoEntries.h"
#include "Connection.h"
#include "core/Core.h"
#include "core/ThreadedRepository.h"
#include "core/SerializableComponent.h"
#include "utils/StringUtils.h"
#include "VolatileRepositoryData.h"

namespace org::apache::nifi::minifi::core::repository {

class VolatileRepository : public core::ThreadedRepositoryImpl {
 public:
  explicit VolatileRepository(std::string_view repo_name = "",
                              std::string /*dir*/ = REPOSITORY_DIRECTORY,
                              std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
                              int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
                              std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
    : core::ThreadedRepositoryImpl(repo_name.length() > 0 ? repo_name : core::className<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod),
      repo_data_(10000, static_cast<size_t>(maxPartitionBytes * 0.75)),
      current_index_(0),
      logger_(logging::LoggerFactory<VolatileRepository>::getLogger()) {
  }

  bool initialize(const std::shared_ptr<Configure> &configure) override;

  bool isNoop() const override {
    return false;
  }

  /**
   * Places a new object into the volatile memory area
   * @param key key to add to the repository
   * @param buf buffer
   **/
  bool Put(const std::string& key, const uint8_t *buf, size_t bufLen) override;

  /**
   * Places new objects into the volatile memory area
   * @param data the key-value pairs to add to the repository
   **/
  bool MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>>& data) override;

  /**
   * Deletes the key
   * @return status of the delete operation
   */
  bool Delete(const std::string& key) override;

  /**
   * Sets the value from the provided key. Once the item is retrieved
   * it may not be retrieved again.
   * @return status of the get operation.
   */
  bool Get(const std::string& key, std::string &value) override;

  uint64_t getRepositorySize() const override {
    return repo_data_.getRepositorySize();
  }

  uint64_t getMaxRepositorySize() const override {
    return repo_data_.getMaxRepositorySize();
  }

  uint64_t getRepositoryEntryCount() const override {
    return repo_data_.getRepositoryEntryCount();
  }

  bool isFull() const override {
    return repo_data_.isFull();
  }

 protected:
  virtual void emplace(RepoValue<std::string> &old_value) {
    std::lock_guard<std::mutex> lock(purge_mutex_);
    purge_list_.push_back(old_value.getKey());
  }

  VolatileRepositoryData repo_data_;
  std::atomic<uint32_t> current_index_;
  std::mutex purge_mutex_;
  std::vector<std::string> purge_list_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::repository
