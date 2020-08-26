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

#include <map>
#include <memory>
#include <string>

#include "core/ContentRepository.h"
#include "core/ContentSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::string ContentRepository::getStoragePath() const {
  return directory_;
}

std::shared_ptr<ContentSession> ContentRepository::createSession() {
  return std::make_shared<ContentSession>(shared_from_this());
}

bool ContentRepository::removeIfOrphaned(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  const std::string str = streamId->getContentFullPath();
  auto count = count_map_.find(str);
  if (count != count_map_.end()) {
    if (count_map_[str] == 0) {
      remove(streamId);
      count_map_.erase(str);
      return true;
    } else {
      return false;
    }
  } else {
    remove(streamId);
    return true;
  }
}

uint32_t ContentRepository::getStreamCount(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  auto cnt = count_map_.find(streamId->getContentFullPath());
  if (cnt != count_map_.end()) {
    return cnt->second;
  } else {
    return 0;
  }
}

void ContentRepository::incrementStreamCount(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  const std::string str = streamId->getContentFullPath();
  auto count = count_map_.find(str);
  if (count != count_map_.end()) {
    count_map_[str] = count->second + 1;
  } else {
    count_map_[str] = 1;
  }
}

void ContentRepository::decrementStreamCount(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
  std::lock_guard<std::mutex> lock(count_map_mutex_);
  const std::string str = streamId->getContentFullPath();
  auto count = count_map_.find(str);
  if (count != count_map_.end() && count->second > 0) {
    count_map_[str] = count->second - 1;
  } else {
    count_map_.erase(str);
  }
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
