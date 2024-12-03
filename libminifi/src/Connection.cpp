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
#include <vector>
#include <memory>
#include <string>
#include <set>
#include <chrono>
#include <thread>
#include <list>
#include "core/FlowFile.h"
#include "core/Connectable.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

ConnectionImpl::ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name)
    : core::ConnectableImpl(name),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  logger_->log_debug("Connection {} created", name_);
}

ConnectionImpl::ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name, const utils::Identifier &uuid)
    : core::ConnectableImpl(name, uuid),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  logger_->log_debug("Connection {} created", name_);
}

ConnectionImpl::ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name, const utils::Identifier &uuid,
                       const utils::Identifier& srcUUID)
    : core::ConnectableImpl(name, uuid),
      src_uuid_(srcUUID),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  logger_->log_debug("Connection {} created", name_);
}

ConnectionImpl::ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::string_view name, const utils::Identifier &uuid,
                       const utils::Identifier& srcUUID, const utils::Identifier& destUUID)
    : core::ConnectableImpl(name, uuid),
      src_uuid_(srcUUID),
      dest_uuid_(destUUID),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)) {
  logger_->log_debug("Connection {} created", name_);
}

ConnectionImpl::ConnectionImpl(std::shared_ptr<core::Repository> flow_repository, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<SwapManager> swap_manager,
                       std::string_view name, const utils::Identifier& uuid)
    : core::ConnectableImpl(name, uuid),
      flow_repository_(std::move(flow_repository)),
      content_repo_(std::move(content_repo)),
      queue_(std::move(swap_manager)) {
  logger_->log_debug("Connection {} created", name_);
}

bool ConnectionImpl::isEmpty() const {
  std::lock_guard<std::mutex> lock(mutex_);

  return queue_.empty();
}

bool ConnectionImpl::backpressureThresholdReached() const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto backpressure_threshold_count = backpressure_threshold_count_.load();
  auto backpressure_threshold_data_size = backpressure_threshold_data_size_.load();

  if (backpressure_threshold_count != 0 && queue_.size() >= backpressure_threshold_count)
    return true;

  if (backpressure_threshold_data_size != 0 && queued_data_size_ >= backpressure_threshold_data_size)
    return true;

  return false;
}

void ConnectionImpl::put(const std::shared_ptr<core::FlowFile>& flow) {
  if (drop_empty_ && flow->getSize() == 0) {
    logger_->log_info("Dropping empty flow file: {}", flow->getUUIDStr());
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);

    queue_.push(flow);

    queued_data_size_ += flow->getSize();

    logger_->log_debug("Enqueue flow file UUID {} to connection {}", flow->getUUIDStr(), name_);
  }

  // Notify receiving processor that work may be available
  if (dest_connectable_) {
    logger_->log_debug("Notifying {} that {} was inserted", dest_connectable_->getName(), flow->getUUIDStr());
    dest_connectable_->notifyWork();
  }
}

void ConnectionImpl::multiPut(std::vector<std::shared_ptr<core::FlowFile>>& flows) {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &ff : flows) {
      if (drop_empty_ && ff->getSize() == 0) {
        logger_->log_info("Dropping empty flow file: {}", ff->getUUIDStr());
        continue;
      }

      queue_.push(ff);
      queued_data_size_ += ff->getSize();

      logger_->log_debug("Enqueue flow file UUID {} to connection {}", ff->getUUIDStr(), name_);
    }
  }

  if (dest_connectable_) {
    logger_->log_debug("Notifying {} that flowfiles were inserted", dest_connectable_->getName());
    dest_connectable_->notifyWork();
  }
}

std::shared_ptr<core::FlowFile> ConnectionImpl::poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords) {
  std::lock_guard<std::mutex> lock(mutex_);

  while (queue_.isWorkAvailable()) {
    std::optional<std::shared_ptr<core::FlowFile>> opt_item = queue_.tryPop();
    if (!opt_item) {
      return nullptr;
    }
    std::shared_ptr<core::FlowFile> item = std::move(opt_item.value());
    queued_data_size_ -= item->getSize();

    if (expired_duration_.load() > 0ms) {
      // We need to check for flow expiration
      if (std::chrono::system_clock::now() > (item->getEntryDate() + expired_duration_.load())) {
        // Flow record expired
        expiredFlowRecords.insert(item);
        logger_->log_debug("Delete flow file UUID {} from connection {}, because it expired", item->getUUIDStr(), name_);
      } else {
        item->setConnection(this);
        logger_->log_debug("Dequeue flow file UUID {} from connection {}", item->getUUIDStr(), name_);
        return item;
      }
    } else {
      item->setConnection(this);
      logger_->log_debug("Dequeue flow file UUID {} from connection {}", item->getUUIDStr(), name_);
      return item;
    }
  }

  return nullptr;
}

void ConnectionImpl::drain(bool delete_permanently) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!delete_permanently) {
    // simply discard in-memory flow files
    queue_.clear();
  } else {
    while (!queue_.empty()) {
      auto opt_item = queue_.tryPop(std::chrono::milliseconds{100});
      if (!opt_item) {
        continue;
      }
      auto& item = opt_item.value();
      if (item->isStored() && flow_repository_->Delete(item->getUUIDStr())) {
        item->setStoredToRepository(false);
        auto claim = item->getResourceClaim();
        if (claim) claim->decreaseFlowFileRecordOwnedCount();
      }
    }
  }

  queued_data_size_ = 0;
  logger_->log_debug("Drain connection {}", name_);
}

}  // namespace org::apache::nifi::minifi
