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
#include "FlowFileRepository.h"
#include "rocksdb/write_batch.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

void FlowFileRepository::flush() {
  rocksdb::WriteBatch batch;
  std::string key;
  std::string value;
  rocksdb::ReadOptions options;

  std::vector<std::shared_ptr<FlowFileRecord>> purgeList;

  uint64_t decrement_total = 0;
  while (keys_to_delete.size_approx() > 0) {
    if (keys_to_delete.try_dequeue(key)) {
      db_->Get(options, key, &value);
      decrement_total += value.size();
      std::shared_ptr<FlowFileRecord> eventRead = std::make_shared<FlowFileRecord>(shared_from_this(), content_repo_);
      if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(value.data()), value.size())) {
        purgeList.push_back(eventRead);
      }
      logger_->log_info("Issuing batch delete, including %s, Content path %s", eventRead->getUUIDStr(), eventRead->getContentFullPath());
      batch.Delete(key);
    }
  }
  if (db_->Write(rocksdb::WriteOptions(), &batch).ok()) {
    logger_->log_info("Decrementing %u from a repo size of %u", decrement_total, repo_size_.load());
    if (decrement_total > repo_size_.load()) {
      repo_size_ = 0;
    } else {
      repo_size_ -= decrement_total;
    }
  }

  if (nullptr != content_repo_) {
    for (const auto &ffr : purgeList) {
      auto claim = ffr->getResourceClaim();
      if (claim != nullptr) {
        content_repo_->removeIfOrphaned(claim);
      }
    }
  }
}

void FlowFileRepository::run() {
  // threshold for purge
  uint64_t purgeThreshold = max_partition_bytes_ * 3 / 4;

  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    uint64_t curTime = getTimeMillis();

    flush();

    uint64_t size = getRepoSize();

    if (size > max_partition_bytes_)
      repo_full_ = true;
    else
      repo_full_ = false;
  }
}

void FlowFileRepository::loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  content_repo_ = content_repo;
  std::vector<std::pair<std::string, uint64_t>> purgeList;
  rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());

  repo_size_ = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::shared_ptr<FlowFileRecord> eventRead = std::make_shared<FlowFileRecord>(shared_from_this(), content_repo_);
    std::string key = it->key().ToString();
    repo_size_ += it->value().size();
    if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(it->value().data()), it->value().size())) {
      logger_->log_info("Found connection for %s, path %s ", eventRead->getConnectionUuid(), eventRead->getContentFullPath());
      auto search = connectionMap.find(eventRead->getConnectionUuid());
      if (search != connectionMap.end()) {
        // we find the connection for the persistent flowfile, create the flowfile and enqueue that
        std::shared_ptr<core::FlowFile> flow_file_ref = std::static_pointer_cast<core::FlowFile>(eventRead);
        eventRead->setStoredToRepository(true);
        search->second->put(eventRead);
      } else {
        logger_->log_info("Could not find connectinon for %s, path %s ", eventRead->getConnectionUuid(), eventRead->getContentFullPath());
        if (eventRead->getContentFullPath().length() > 0) {
          if (nullptr != eventRead->getResourceClaim()) {
            content_repo_->remove(eventRead->getResourceClaim());
          }
        }
        purgeList.push_back(std::make_pair(key, it->value().size()));
      }
    } else {
      purgeList.push_back(std::make_pair(key, it->value().size()));
    }
  }

  delete it;
  for (auto eventId : purgeList) {
    logger_->log_info("Repository Repo %s Purge %s", name_.c_str(), eventId.first.c_str());
    if (Delete(eventId.first)) {
      repo_size_ -= eventId.second;
    }
  }

  return;
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
