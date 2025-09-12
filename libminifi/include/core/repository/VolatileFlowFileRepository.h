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

#include <memory>
#include <string>
#include <string_view>

#include "VolatileRepository.h"
#include "FlowFileRecord.h"
#include "minifi-cpp/utils/gsl.h"

struct VolatileFlowFileRepositoryTestAccessor;

namespace org::apache::nifi::minifi::core::repository {

/**
 * Volatile flow file repository. keeps a running counter of the current location, freeing
 * those which we no longer hold.
 */
class VolatileFlowFileRepository : public VolatileRepository {
  friend struct ::VolatileFlowFileRepositoryTestAccessor;

 public:
  explicit VolatileFlowFileRepository(std::string_view repo_name = "",
                                      const std::string& /*dir*/ = REPOSITORY_DIRECTORY,
                                      std::chrono::milliseconds maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME,
                                      int64_t maxPartitionBytes = MAX_REPOSITORY_STORAGE_SIZE,
                                      std::chrono::milliseconds purgePeriod = REPOSITORY_PURGE_PERIOD)
    : VolatileRepository(repo_name.length() > 0 ? repo_name : core::className<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod) {
  }

  ~VolatileFlowFileRepository() override {
    stop();
  }

 private:
  void run() override {
    while (isRunning()) {
      std::this_thread::sleep_for(purge_period_);
      flush();
    }
    flush();
  }

  std::thread& getThread() override {
    return thread_;
  }

  void flush() override {
    if (!content_repo_) {
      return;
    }
    std::lock_guard<std::mutex> lock(purge_mutex_);
    for (auto purgeItem : purge_list_) {
      utils::Identifier containerId;
      auto eventRead = FlowFileRecord::DeSerialize(gsl::make_span(purgeItem).as_span<const std::byte>(), content_repo_, containerId);
      if (eventRead) {
        auto claim = eventRead->getResourceClaim();
        if (claim) claim->decreaseFlowFileRecordOwnedCount();
      }
    }
    purge_list_.resize(0);
    purge_list_.clear();
  }

  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) override {
    content_repo_ = content_repo;
  }

  void emplace(RepoValue<std::string> &old_value) override {
    std::string buffer;
    old_value.emplace(buffer);
    std::lock_guard<std::mutex> lock(purge_mutex_);
    purge_list_.push_back(buffer);
  }

  std::shared_ptr<core::ContentRepository> content_repo_;
  std::thread thread_;
};
}  // namespace org::apache::nifi::minifi::core::repository
