/**
 * @file Connection.cpp
 * Connection class implementation
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
#include "Connection.h"
#include <ctime>
#include <vector>
#include <queue>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <iostream>
#include <list>
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

Connection::Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name)
    : core::Connectable(name),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  logger_->log_debug("Connection %s created", name_);
}

Connection::Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name, const utils::Identifier &uuid)
    : core::Connectable(name, uuid),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  logger_->log_debug("Connection %s created", name_);
}

Connection::Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name, const utils::Identifier &uuid,
                       const utils::Identifier& srcUUID)
    : core::Connectable(name, uuid),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  src_uuid_ = srcUUID;
  logger_->log_debug("Connection %s created", name_);
}

Connection::Connection(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, const std::string &name, const utils::Identifier &uuid,
                       const utils::Identifier& srcUUID, const utils::Identifier& destUUID)
    : core::Connectable(name, uuid),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  src_uuid_ = srcUUID;
  dest_uuid_ = destUUID;
  logger_->log_debug("Connection %s created", name_);
}

bool Connection::isEmpty() const {
  std::lock_guard<std::mutex> lock(mutex_);

  return queue_.empty();
}

bool Connection::isFull() const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (max_queue_size_ <= 0 && max_data_queue_size_ <= 0)
    // No back pressure setting
    return false;

  if (max_queue_size_ > 0 && queue_.size() >= max_queue_size_)
    return true;

  if (max_data_queue_size_ > 0 && queued_data_size_ >= max_data_queue_size_)
    return true;

  return false;
}

void Connection::put(const std::shared_ptr<core::FlowFile>& flow) {
  if (drop_empty_ && flow->getSize() == 0) {
    logger_->log_info("Dropping empty flow file: %s", flow->getUUIDStr());
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);

    queue_.push(flow);

    queued_data_size_ += flow->getSize();

    logger_->log_debug("Enqueue flow file UUID %s to connection %s", flow->getUUIDStr(), name_);
  }

  // Notify receiving processor that work may be available
  if (dest_connectable_) {
    logger_->log_debug("Notifying %s that %s was inserted", dest_connectable_->getName(), flow->getUUIDStr());
    dest_connectable_->notifyWork();
  }
}

void Connection::multiPut(std::vector<std::shared_ptr<core::FlowFile>>& flows) {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &ff : flows) {
      if (drop_empty_ && ff->getSize() == 0) {
        logger_->log_info("Dropping empty flow file: %s", ff->getUUIDStr());
        continue;
      }

      queue_.push(ff);
      queued_data_size_ += ff->getSize();

      logger_->log_debug("Enqueue flow file UUID %s to connection %s", ff->getUUIDStr(), name_);
    }
  }

  if (dest_connectable_) {
    logger_->log_debug("Notifying %s that flowfiles were inserted", dest_connectable_->getName());
    dest_connectable_->notifyWork();
  }
}

std::shared_ptr<core::FlowFile> Connection::poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords) {
  std::lock_guard<std::mutex> lock(mutex_);

  while (queue_.isWorkAvailable()) {
    std::shared_ptr<core::FlowFile> item = queue_.pop();
    queued_data_size_ -= item->getSize();

    if (expired_duration_.load() > 0ms) {
      // We need to check for flow expiration
      if (std::chrono::system_clock::now() > (item->getEntryDate() + expired_duration_.load())) {
        // Flow record expired
        expiredFlowRecords.insert(item);
        logger_->log_debug("Delete flow file UUID %s from connection %s, because it expired", item->getUUIDStr(), name_);
      } else {
        item->setConnection(this);
        logger_->log_debug("Dequeue flow file UUID %s from connection %s", item->getUUIDStr(), name_);
        return item;
      }
    } else {
      item->setConnection(this);
      logger_->log_debug("Dequeue flow file UUID %s from connection %s", item->getUUIDStr(), name_);
      return item;
    }
  }

  return nullptr;
}

void Connection::drain(bool delete_permanently) {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!queue_.empty()) {
    std::shared_ptr<core::FlowFile> item = queue_.pop();
    logger_->log_debug("Delete flow file UUID %s from connection %s, because it expired", item->getUUIDStr(), name_);
    if (delete_permanently) {
      if (item->isStored() && flow_repository_->Delete(item->getUUIDStr())) {
        item->setStoredToRepository(false);
        auto claim = item->getResourceClaim();
        if (claim) claim->decreaseFlowFileRecordOwnedCount();
      }
    }
  }
  queued_data_size_ = 0;
  logger_->log_debug("Drain connection %s", name_);
}

}  // namespace org::apache::nifi::minifi
