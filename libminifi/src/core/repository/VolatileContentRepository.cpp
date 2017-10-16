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
#include <cstdio>
#include <string>
#include <memory>
#include <thread>
#include "utils/StringUtils.h"
#include "io/FileStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

const char *VolatileContentRepository::minimal_locking = "minimal.locking";

bool VolatileContentRepository::initialize(const std::shared_ptr<Configure> &configure) {
  VolatileRepository::initialize(configure);
  resource_claim_comparator_ = [](std::shared_ptr<minifi::ResourceClaim> lhsPtr, std::shared_ptr<minifi::ResourceClaim> rhsPtr) {
    if (lhsPtr == nullptr || rhsPtr == nullptr) {
      return false;
    }
    return lhsPtr->getContentFullPath() == rhsPtr->getContentFullPath();};
  resource_claim_check_ = [](std::shared_ptr<minifi::ResourceClaim> claim) {
    return claim->getFlowFileRecordOwnedCount() <= 0;};
  claim_reclaimer_ = [&](std::shared_ptr<minifi::ResourceClaim> claim) {if (claim->getFlowFileRecordOwnedCount() <= 0) {
      remove(claim);
    }
  };

  if (configure != nullptr) {
    bool minimize_locking = false;
    std::string value;
    std::stringstream strstream;
    strstream << Configure::nifi_volatile_repository_options << getName() << "." << minimal_locking;
    if (configure->get(strstream.str(), value)) {
      utils::StringUtils::StringToBool(value, minimize_locking);
      minimize_locking_ = minimize_locking;
    }
  }
  if (!minimize_locking_) {
    for (auto ent : value_vector_) {
      delete ent;
    }
    value_vector_.clear();
  }
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
  logger_->log_info("%s Repository Monitor Thread Start", getName());
}

std::shared_ptr<io::BaseStream> VolatileContentRepository::write(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter write for %s", claim->getContentFullPath());
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto claim_check = master_list_.find(claim->getContentFullPath());
    if (claim_check != master_list_.end()) {
      logger_->log_debug("Creating copy of atomic entry");
      auto ent = claim_check->second->takeOwnership();
      if (ent == nullptr) {
        logger_->log_debug("write returns nullptr");
        return nullptr;
      }
      return std::make_shared<io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, ent);
    }
  }

  int size = 0;
  if (__builtin_expect(minimize_locking_ == true, 1)) {
    logger_->log_debug("Minimize locking");
    for (auto ent : value_vector_) {
      if (ent->testAndSetKey(claim, nullptr, nullptr, resource_claim_comparator_)) {
        std::lock_guard<std::mutex> lock(map_mutex_);
        master_list_[claim->getContentFullPath()] = ent;
        return std::make_shared<io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, ent);
      }
      size++;
    }
  } else {
    std::lock_guard < std::mutex > lock(map_mutex_);
    auto claim_check = master_list_.find(claim->getContentFullPath());
    if (claim_check != master_list_.end()) {
      logger_->log_debug("Creating copy of atomic entry");
      return std::make_shared < io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, claim_check->second);
    } else {
      logger_->log_debug("Creating new atomic entry");
      AtomicEntry<std::shared_ptr<minifi::ResourceClaim>> *ent = new AtomicEntry<std::shared_ptr<minifi::ResourceClaim>>(&current_size_, &max_size_);
      if (ent->testAndSetKey(claim, nullptr, nullptr, resource_claim_comparator_)) {
        master_list_[claim->getContentFullPath()] = ent;
        return std::make_shared< io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, ent);
      }
    }
  }
  logger_->log_debug("write returns nullptr %d", size);
  return nullptr;
}

bool VolatileContentRepository::exists(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter exists for %s", claim->getContentFullPath());
  int size = 0;
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto claim_check = master_list_.find(claim->getContentFullPath());
    if (claim_check != master_list_.end()) {
      auto ent = claim_check->second->takeOwnership();
      if (ent == nullptr) {
        return false;
      }
      return true;
    }
  }
  return false;
}

std::shared_ptr<io::BaseStream> VolatileContentRepository::read(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter read for %s", claim->getContentFullPath());
  int size = 0;
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto claim_check = master_list_.find(claim->getContentFullPath());
    if (claim_check != master_list_.end()) {
      auto ent = claim_check->second->takeOwnership();
      if (ent == nullptr) {
        return nullptr;
      }
      return std::make_shared<io::AtomicEntryStream<std::shared_ptr<minifi::ResourceClaim>>>(claim, ent);
    }
  }
  logger_->log_debug("enter read for %s after %d", claim->getContentFullPath(), size);
  return nullptr;
}

bool VolatileContentRepository::remove(const std::shared_ptr<minifi::ResourceClaim> &claim) {
  logger_->log_debug("enter remove for %s, reducing %d", claim->getContentFullPath(), current_size_.load());
  if (__builtin_expect(minimize_locking_ == true, 1)) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto ent = master_list_.find(claim->getContentFullPath());
    if (ent != master_list_.end()) {
      // if we cannot remove the entry we will let the owner's destructor
      // decrement the reference count and free it
      master_list_.erase(claim->getContentFullPath());
      if (ent->second->freeValue(claim)) {
        logger_->log_debug("removed %s", claim->getContentFullPath());
        logger_->log_debug("Remove for %s, reduced to %d", claim->getContentFullPath(), current_size_.load());
        return true;
      } else {
        logger_->log_debug("free failed for %s", claim->getContentFullPath());
      }
    } else {
      logger_->log_debug("Could not remove for %s, size is %d", claim->getContentFullPath(), current_size_.load());
    }
  } else {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto size = master_list_[claim->getContentFullPath()]->getLength();
    delete master_list_[claim->getContentFullPath()];
    master_list_.erase(claim->getContentFullPath());
    current_size_ -= size;
    logger_->log_debug("Remove for %s, reduced to %d", claim->getContentFullPath(), current_size_.load());
    return true;
  }

  logger_->log_debug("Remove for %s, reduced to %d", claim->getContentFullPath(), current_size_.load());
  logger_->log_debug("could not remove %s", claim->getContentFullPath());
  return false;
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
