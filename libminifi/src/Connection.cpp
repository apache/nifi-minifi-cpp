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
#include <time.h>
#include <vector>
#include <queue>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <iostream>
#include "core/FlowFile.h"
#include "Connection.h"
#include "core/Processor.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

Connection::Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name)
    : core::Connectable(name),
      flow_repository_(flow_repository),
      content_repo_(content_repo),
      logger_(logging::LoggerFactory<Connection>::getLogger()) {
  source_connectable_ = nullptr;
  dest_connectable_ = nullptr;
  max_queue_size_ = 0;
  max_data_queue_size_ = 0;
  expired_duration_ = 0;
  queued_data_size_ = 0;

  logger_->log_debug("Connection %s created", name_);
}

Connection::Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name, utils::Identifier & uuid)
    : core::Connectable(name, uuid),
      flow_repository_(flow_repository),
      content_repo_(content_repo),
      logger_(logging::LoggerFactory<Connection>::getLogger()) {
  source_connectable_ = nullptr;
  dest_connectable_ = nullptr;
  max_queue_size_ = 0;
  max_data_queue_size_ = 0;
  expired_duration_ = 0;
  queued_data_size_ = 0;

  logger_->log_debug("Connection %s created", name_);
}

Connection::Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name, utils::Identifier & uuid,
                       utils::Identifier & srcUUID)
    : core::Connectable(name, uuid),
      flow_repository_(flow_repository),
      content_repo_(content_repo),
      logger_(logging::LoggerFactory<Connection>::getLogger()) {

  src_uuid_ = srcUUID;

  source_connectable_ = nullptr;
  dest_connectable_ = nullptr;
  max_queue_size_ = 0;
  max_data_queue_size_ = 0;
  expired_duration_ = 0;
  queued_data_size_ = 0;

  logger_->log_debug("Connection %s created", name_);
}

Connection::Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name, utils::Identifier & uuid,
                       utils::Identifier & srcUUID, utils::Identifier & destUUID)
    : core::Connectable(name, uuid),
      flow_repository_(flow_repository),
      content_repo_(content_repo),
      logger_(logging::LoggerFactory<Connection>::getLogger()) {

  src_uuid_ = srcUUID;
  dest_uuid_ = destUUID;

  source_connectable_ = nullptr;
  dest_connectable_ = nullptr;
  max_queue_size_ = 0;
  max_data_queue_size_ = 0;
  expired_duration_ = 0;
  queued_data_size_ = 0;

  logger_->log_debug("Connection %s created", name_);
}

bool Connection::isEmpty() {
  std::lock_guard<std::mutex> lock(mutex_);

  return queue_.empty();
}

bool Connection::isFull() {
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

void Connection::put(std::shared_ptr<core::FlowFile> flow) {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    queue_.push(flow);

    queued_data_size_ += flow->getSize();

    logger_->log_debug("Enqueue flow file UUID %s to connection %s", flow->getUUIDStr(), name_);
  }

  if (!flow->isStored()) {
    // Save to the flowfile repo
    FlowFileRecord event(flow_repository_, content_repo_, flow, this->uuidStr_);
    if (event.Serialize()) {
      flow->setStoredToRepository(true);
    }
  }

  // Notify receiving processor that work may be available
  if (dest_connectable_) {
    logger_->log_debug("Notifying %s that %s was inserted", dest_connectable_->getName(), flow->getUUIDStr());
    dest_connectable_->notifyWork();
  }
}

std::shared_ptr<core::FlowFile> Connection::poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords) {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!queue_.empty()) {
    std::shared_ptr<core::FlowFile> item = queue_.front();
    queue_.pop();
    queued_data_size_ -= item->getSize();

    if (expired_duration_ > 0) {
      // We need to check for flow expiration
      if (getTimeMillis() > (item->getEntryDate() + expired_duration_)) {
        // Flow record expired
        expiredFlowRecords.insert(item);
        logger_->log_debug("Delete flow file UUID %s from connection %s, because it expired", item->getUUIDStr(), name_);
        if (flow_repository_->Delete(item->getUUIDStr())) {
          item->setStoredToRepository(false);
        }
      } else {
        // Flow record not expired
        if (item->isPenalized()) {
          // Flow record was penalized
          queue_.push(item);
          queued_data_size_ += item->getSize();
          break;
        }
        std::shared_ptr<Connectable> connectable = std::static_pointer_cast<Connectable>(shared_from_this());
        item->setOriginalConnection(connectable);
        logger_->log_debug("Dequeue flow file UUID %s from connection %s", item->getUUIDStr(), name_);
        return item;
      }
    } else {
      // Flow record not expired
      if (item->isPenalized()) {
        // Flow record was penalized
        queue_.push(item);
        queued_data_size_ += item->getSize();
        break;
      }
      std::shared_ptr<Connectable> connectable = std::static_pointer_cast<Connectable>(shared_from_this());
      item->setOriginalConnection(connectable);
      logger_->log_debug("Dequeue flow file UUID %s from connection %s", item->getUUIDStr(), name_);
      return item;
    }
  }

  return NULL;
}

void Connection::drain() {
  std::lock_guard<std::mutex> lock(mutex_);

  while (!queue_.empty()) {
    std::shared_ptr<core::FlowFile> item = queue_.front();
    queue_.pop();
    logger_->log_debug("Delete flow file UUID %s from connection %s, because it expired", item->getUUIDStr(), name_);
    if (flow_repository_->Delete(item->getUUIDStr())) {
      item->setStoredToRepository(false);
    }
  }
  queued_data_size_ = 0;
  logger_->log_debug("Drain connection %s", name_);
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
