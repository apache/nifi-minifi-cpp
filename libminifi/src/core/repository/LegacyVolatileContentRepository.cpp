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

#include "core/repository/LegacyVolatileContentRepository.h"

#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#include "core/expect.h"
#include "io/FileStream.h"
#include "utils/StringUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::repository {

const char *LegacyVolatileContentRepository::minimal_locking = "minimal.locking";

bool LegacyVolatileContentRepository::initialize(const std::shared_ptr<Configure> &configure) {
  repo_data_.initialize(configure, getName());

  logger_->log_info("Resizing repo_data_.value_vector for {} count is {}", getName(), repo_data_.max_count);
  logger_->log_info("Using a maximum size for {} of {}", getName(), repo_data_.max_size);

  if (configure != nullptr) {
    std::string value;
    std::stringstream strstream;
    strstream << Configure::nifi_volatile_repository_options << getName() << "." << minimal_locking;
    if (configure->get(strstream.str(), value)) {
      minimize_locking_ =  utils::string::toBool(value).value_or(true);
    }
  }
  if (!minimize_locking_) {
    repo_data_.clear();
  }

  return true;
}

std::shared_ptr<io::BaseStream> LegacyVolatileContentRepository::write(const minifi::ResourceClaim &claim, bool /*append*/) {
  logger_->log_info("enter write for {}", claim.getContentFullPath());
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto claim_check = master_list_.find(claim.getContentFullPath());
    if (claim_check != master_list_.end()) {
      logger_->log_info("Creating copy of atomic entry");
      auto ent = claim_check->second->takeOwnership();
      if (ent == nullptr) {
        return nullptr;
      }
      return std::make_shared<io::AtomicEntryStream<ResourceClaim::Path>>(claim.getContentFullPath(), ent);
    }
  }

  int size = 0;
  if (LIKELY(minimize_locking_ == true)) {
    for (auto ent : repo_data_.value_vector) {
      if (ent->testAndSetKey(claim.getContentFullPath())) {
        std::lock_guard<std::mutex> lock(map_mutex_);
        master_list_[claim.getContentFullPath()] = ent;
        logger_->log_info("Minimize locking, return stream for {}", claim.getContentFullPath());
        return std::make_shared<io::AtomicEntryStream<ResourceClaim::Path>>(claim.getContentFullPath(), ent);
      }
      size++;
    }
  } else {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto claim_check = master_list_.find(claim.getContentFullPath());
    if (claim_check != master_list_.end()) {
      return std::make_shared<io::AtomicEntryStream<ResourceClaim::Path>>(claim.getContentFullPath(), claim_check->second);
    } else {
      auto *ent = new AtomicEntry<ResourceClaim::Path>(&repo_data_.current_size, &repo_data_.max_size);  // NOLINT(cppcoreguidelines-owning-memory)
      if (ent->testAndSetKey(claim.getContentFullPath())) {
        master_list_[claim.getContentFullPath()] = ent;
        return std::make_shared<io::AtomicEntryStream<ResourceClaim::Path>>(claim.getContentFullPath(), ent);
      }
    }
  }
  logger_->log_info("Cannot write {} {}, returning nullptr to roll back session. Repo is either full or locked", claim.getContentFullPath(), size);
  return nullptr;
}

bool LegacyVolatileContentRepository::exists(const minifi::ResourceClaim &claim) {
  std::lock_guard<std::mutex> lock(map_mutex_);
  auto claim_check = master_list_.find(claim.getContentFullPath());
  if (claim_check != master_list_.end()) {
    auto ent = claim_check->second->takeOwnership();
    return ent != nullptr;
  }

  return false;
}

std::shared_ptr<io::BaseStream> LegacyVolatileContentRepository::read(const minifi::ResourceClaim &claim) {
  std::lock_guard<std::mutex> lock(map_mutex_);
  auto claim_check = master_list_.find(claim.getContentFullPath());
  if (claim_check != master_list_.end()) {
    auto ent = claim_check->second->takeOwnership();
    if (ent == nullptr) {
      return nullptr;
    }
    return std::make_shared<io::AtomicEntryStream<ResourceClaim::Path>>(claim.getContentFullPath(), ent);
  }

  return nullptr;
}

bool LegacyVolatileContentRepository::removeKey(const std::string& content_path) {
  if (LIKELY(minimize_locking_ == true)) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto ent = master_list_.find(content_path);
    if (ent != master_list_.end()) {
      auto ptr = ent->second;
      // if we cannot remove the entry we will let the owner's destructor
      // decrement the reference count and free it
      master_list_.erase(content_path);
      // because of the test and set we need to decrement ownership
      ptr->decrementOwnership();
      if (ptr->freeValue(content_path)) {
        logger_->log_info("Deleting resource {}", content_path);
      } else {
        logger_->log_info("free failed for {}", content_path);
      }
    } else {
      logger_->log_info("Could not remove {}", content_path);
    }
  } else {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto claim_item = master_list_.find(content_path);
    if (claim_item != master_list_.end()) {
      auto size = claim_item->second->getLength();
      delete claim_item->second;  // NOLINT(cppcoreguidelines-owning-memory)
      master_list_.erase(content_path);
      repo_data_.current_size -= size;
    }
  }

  logger_->log_info("Could not remove {}, may not exist", content_path);
  return true;
}

}  // namespace org::apache::nifi::minifi::core::repository
