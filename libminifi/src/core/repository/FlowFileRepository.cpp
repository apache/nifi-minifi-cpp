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
#include "../../../include/core/repository/FlowFileRepository.h"

#include <memory>
#include <string>
#include <vector>

#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

void FlowFileRepository::run() {
  // threshold for purge
  uint64_t purgeThreshold = max_partition_bytes_ * 3 / 4;
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    uint64_t curTime = getTimeMillis();
    uint64_t size = repoSize();
    if (size >= purgeThreshold) {
      std::vector<std::string> purgeList;
      leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());

      for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::shared_ptr<FlowFileRecord> eventRead = std::make_shared < FlowFileRecord > (shared_from_this(), content_repo_);
        std::string key = it->key().ToString();
        if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(it->value().data()), it->value().size())) {
          if ((curTime - eventRead->getEventTime()) > max_partition_millis_)
            purgeList.push_back(key);
        } else {
          logger_->log_debug("NiFi %s retrieve event %s fail", name_, key);
          purgeList.push_back(key);
        }
      }
      delete it;
      for (auto eventId : purgeList) {
        logger_->log_info("Repository Repo %s Purge %s", name_, eventId);
        Delete(eventId);
      }
    }
    if (size > max_partition_bytes_)
      repo_full_ = true;
    else
      repo_full_ = false;
  }
  return;
}

void FlowFileRepository::loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  content_repo_ = content_repo;
  std::vector<std::string> purgeList;
  leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());

  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::shared_ptr<FlowFileRecord> eventRead = std::make_shared < FlowFileRecord > (shared_from_this(), content_repo_);
    std::string key = it->key().ToString();
    if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(it->value().data()), it->value().size())) {
      auto search = connectionMap.find(eventRead->getConnectionUuid());
      if (search != connectionMap.end()) {
        // we find the connection for the persistent flowfile, create the flowfile and enqueue that
        std::shared_ptr<core::FlowFile> flow_file_ref = std::static_pointer_cast < core::FlowFile > (eventRead);
        std::shared_ptr<FlowFileRecord> record = std::make_shared < FlowFileRecord > (shared_from_this(), content_repo_, flow_file_ref);
        // set store to repo to true so that we do need to persistent again in enqueue
        record->setStoredToRepository(true);
        search->second->put(record);
      } else {
        if (eventRead->getContentFullPath().length() > 0 && content_repo != nullptr) {
          content_repo->remove(eventRead->getResourceClaim());
        }
        purgeList.push_back(key);
      }
    } else {
      purgeList.push_back(key);
    }
  }

  delete it;
  std::vector<std::string>::iterator itPurge;
  for (itPurge = purgeList.begin(); itPurge != purgeList.end(); itPurge++) {
    std::string eventId = *itPurge;
    logger_->log_info("Repository Repo %s Purge %s", name_.c_str(), eventId.c_str());
    Delete(eventId);
  }

  return;
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
