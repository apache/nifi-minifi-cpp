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

#include "core/ContentRepository.h"

#include <map>
#include <memory>
#include <string>

#include "core/BufferedContentSession.h"

namespace org::apache::nifi::minifi::core {

std::string ContentRepositoryImpl::getStoragePath() const {
  return directory_;
}

void ContentRepositoryImpl::reset() {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  count_map_.clear();
}

std::shared_ptr<ContentSession> ContentRepositoryImpl::createSession() {
  return std::make_shared<BufferedContentSession>(sharedFromThis<ContentRepositoryImpl>());
}

uint32_t ContentRepositoryImpl::getStreamCount(const minifi::ResourceClaim &streamId) {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  auto cnt = count_map_.find(streamId.getContentFullPath());
  if (cnt != count_map_.end()) {
    return cnt->second;
  } else {
    return 0;
  }
}

void ContentRepositoryImpl::incrementStreamCount(const minifi::ResourceClaim &streamId) {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  const std::string str = streamId.getContentFullPath();
  auto count = count_map_.find(str);
  if (count != count_map_.end()) {
    count_map_[str] = count->second + 1;
  } else {
    count_map_[str] = 1;
  }
}

void ContentRepositoryImpl::removeFromPurgeList() {
  std::lock_guard<std::mutex> lock(purge_list_mutex_);
  for (auto it = purge_list_.begin(); it != purge_list_.end();) {
    if (removeKey(*it)) {
      purge_list_.erase(it++);
    } else {
      ++it;
    }
  }
}

ContentRepository::StreamState ContentRepositoryImpl::decrementStreamCount(const minifi::ResourceClaim &streamId) {
  {
    std::lock_guard<std::mutex> lock(count_map_mutex_);
    const std::string str = streamId.getContentFullPath();
    auto count = count_map_.find(str);
    if (count != count_map_.end() && count->second > 1) {
      count_map_[str] = count->second - 1;
      return StreamState::Alive;
    }

    count_map_.erase(str);
  }

  remove(streamId);
  return StreamState::Deleted;
}

bool ContentRepositoryImpl::remove(const minifi::ResourceClaim &streamId) {
  removeFromPurgeList();
  if (!removeKey(streamId.getContentFullPath())) {
    std::lock_guard<std::mutex> lock(purge_list_mutex_);
    purge_list_.push_back(streamId.getContentFullPath());
    return false;
  }
  return true;
}

std::unique_ptr<StreamAppendLock> ContentRepositoryImpl::lockAppend(const org::apache::nifi::minifi::ResourceClaim &claim, size_t offset) {
  std::lock_guard guard(appending_mutex_);
  if (offset != size(claim)) {
    // we are trying to append to a resource that has already been appended to
    return {};
  }
  if (!appending_.insert(claim.getContentFullPath()).second) {
    // this resource is currently being appended to
    return {};
  }
  return std::make_unique<ContentStreamAppendLock>(sharedFromThis<ContentRepositoryImpl>(), claim);
}

void ContentRepositoryImpl::unlockAppend(const ResourceClaim::Path &path) {
  std::lock_guard guard(appending_mutex_);
  size_t removed_count = appending_.erase(path);
  gsl_Expects(removed_count == 1);
}

}  // namespace org::apache::nifi::minifi::core
