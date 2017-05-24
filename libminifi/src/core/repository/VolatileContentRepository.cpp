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

#include "core/repository/VolatileContentRepository.h"
#include <memory>
#include <thread>
#include "io/FileStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

bool VolatileContentRepository::initialize(const std::shared_ptr<Configure> &configure) {
  VolatileRepository::initialize(configure);
  resource_claim_comparator_ = [](std::shared_ptr<minifi::ResourceClaim> lhsPtr, std::shared_ptr<minifi::ResourceClaim> rhsPtr) {
    return lhsPtr->getContentFullPath() == rhsPtr->getContentFullPath();};
  resource_claim_check_ = [](std::shared_ptr<minifi::ResourceClaim> claim) {
    return claim->getFlowFileRecordOwnedCount() <= 0;};
  claim_reclaimer_ = [&](std::shared_ptr<minifi::ResourceClaim> claim) {if (claim->getFlowFileRecordOwnedCount() <= 0) {
      remove(claim);
    }
  };
  start();

  return true;
}

void VolatileContentRepository::stop() {
  running_ = false;
}

void VolatileContentRepository::run() {
}

void VolatileContentRepository::start() {
  if (this->purge_period_ <= 0)
    return;
  if (running_)
    return;
  thread_ = std::thread(&VolatileContentRepository::run, shared_from_parent<VolatileContentRepository>());
  thread_.detach();
  running_ = true;
  logger_->log_info("%s Repository Monitor Thread Start", name_);
}

std::shared_ptr<io::BaseStream> VolatileContentRepository::write(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter write");
  {
    std::lock_guard < std::mutex > lock(map_mutex_);
    auto claim_check = master_list_.find(claim->getContentFullPath());
    if (claim_check != master_list_.end()) {
      return std::make_shared < io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, claim_check->second);
    }
  }

  int size = 0;
  for (auto ent : value_vector_) {
    if (ent->testAndSetKey(claim, nullptr, nullptr, resource_claim_comparator_)) {
      std::lock_guard < std::mutex > lock(map_mutex_);
      master_list_[claim->getContentFullPath()] = ent;
      return std::make_shared < io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, ent);
    }
    size++;
  }
  return nullptr;
}

std::shared_ptr<io::BaseStream> VolatileContentRepository::read(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter read");
  int size = 0;
  {
    std::lock_guard < std::mutex > lock(map_mutex_);
    auto claim_check = master_list_.find(claim->getContentFullPath());
    if (claim_check != master_list_.end()) {
      return std::make_shared < io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, claim_check->second);
    }
  }

  for (auto ent : value_vector_) {
    RepoValue<std::shared_ptr<minifi::ResourceClaim>> *repo_value;

    if (ent->getValue(claim, &repo_value)) {
      return std::make_shared < io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, ent);
    }
    size++;
  }
  logger_->log_debug("enter read for %s after %d", claim->getContentFullPath(), size);
  return nullptr;
}

bool VolatileContentRepository::remove(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter remove");

  for (auto ent : value_vector_) {
    // let the destructor do the cleanup
    if (ent->freeValue(claim)) {
      std::lock_guard < std::mutex > lock(map_mutex_);
      master_list_.erase(claim->getContentFullPath());
      logger_->log_debug("removed %s", claim->getContentFullPath());
      return true;
    }
  }
  return false;
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
