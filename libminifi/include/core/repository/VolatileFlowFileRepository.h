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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEFLOWFILEREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEFLOWFILEREPOSITORY_H_

#include "VolatileRepository.h"
#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

/**
 * Volatile flow file repository. keeps a running counter of the current location, freeing
 * those which we no longer hold.
 */
class VolatileFlowFileRepository : public VolatileRepository<std::string> {
 public:
  explicit VolatileFlowFileRepository(std::string repo_name = "", std::string dir = REPOSITORY_DIRECTORY, int64_t maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME, int64_t maxPartitionBytes =
  MAX_REPOSITORY_STORAGE_SIZE,
                                      uint64_t purgePeriod = REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),
        VolatileRepository(repo_name.length() > 0 ? repo_name : core::getClassName<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod)

  {
    purge_required_ = true;
    content_repo_ = nullptr;
  }

  virtual void run() {
    repo_full_ = false;
    while (running_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
      if (purge_required_ && nullptr != content_repo_) {
        std::lock_guard<std::mutex> lock(purge_mutex_);
        for (auto purgeItem : purge_list_) {
          std::shared_ptr<FlowFileRecord> eventRead = std::make_shared<FlowFileRecord>(shared_from_this(), content_repo_);
          if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(purgeItem.data()), purgeItem.size())) {
            std::shared_ptr<minifi::ResourceClaim> newClaim = eventRead->getResourceClaim();
            content_repo_->remove(newClaim);
          }
        }
        purge_list_.resize(0);
        purge_list_.clear();
      }
    }
  }

  void loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
    content_repo_ = content_repo;

  }

 protected:

  virtual void emplace(RepoValue<std::string> &old_value) {
    std::string buffer;
    old_value.emplace(buffer);
    std::lock_guard<std::mutex> lock(purge_mutex_);
    purge_list_.push_back(buffer);
  }

  std::shared_ptr<core::ContentRepository> content_repo_;

};
} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEFLOWFILEREPOSITORY_H_ */
