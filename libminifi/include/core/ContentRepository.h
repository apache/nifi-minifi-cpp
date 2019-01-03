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
#ifndef LIBMINIFI_INCLUDE_CORE_CONTENTREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_CONTENTREPOSITORY_H_

#include "properties/Configure.h"
#include "ResourceClaim.h"
#include "io/DataStream.h"
#include "io/BaseStream.h"
#include "StreamManager.h"
#include "core/Connectable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Content repository definition that extends StreamManager.
 */
class ContentRepository : public StreamManager<minifi::ResourceClaim> {
 public:

  virtual ~ContentRepository() {

  }

  /**
   * initialize this content repository using the provided configuration.
   */
  virtual bool initialize(const std::shared_ptr<Configure> &configure) = 0;

  virtual std::string getStoragePath() {
    return directory_;
  }

  /**
   * Stops this repository.
   */
  virtual void stop() = 0;

  /**
   * Removes an item if it was orphan
   */
  virtual bool removeIfOrphaned(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
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

  virtual uint32_t getStreamCount(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
    std::lock_guard<std::mutex> lock(count_map_mutex_);
    auto cnt = count_map_.find(streamId->getContentFullPath());
    if (cnt != count_map_.end()) {
      return cnt->second;
    } else {
      return 0;
    }
  }

  virtual void incrementStreamCount(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
    std::lock_guard<std::mutex> lock(count_map_mutex_);
    const std::string str = streamId->getContentFullPath();
    auto count = count_map_.find(str);
    if (count != count_map_.end()) {
      count_map_[str] = count->second + 1;
    } else {
      count_map_[str] = 1;
    }
  }

  virtual void decrementStreamCount(const std::shared_ptr<minifi::ResourceClaim> &streamId) {
    std::lock_guard<std::mutex> lock(count_map_mutex_);
    const std::string str = streamId->getContentFullPath();
    auto count = count_map_.find(str);
    if (count != count_map_.end() && count->second > 0) {
      count_map_[str] = count->second - 1;
    } else {
      count_map_[str] = 0;
    }
  }

 protected:

  std::string directory_;

  std::mutex count_map_mutex_;

  std::map<std::string, uint32_t> count_map_;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONTENTREPOSITORY_H_ */
