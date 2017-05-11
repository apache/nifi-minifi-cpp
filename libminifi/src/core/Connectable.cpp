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
#include "core/Connectable.h"
#include <uuid/uuid.h>
#include <utility>
#include <memory>
#include <string>
#include <set>
#include "core/logging/LoggerConfiguration.h"
#include "core/Relationship.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

Connectable::Connectable(std::string name, uuid_t uuid)
    : CoreComponent(name, uuid),
      max_concurrent_tasks_(1),
      logger_(logging::LoggerFactory<Connectable>::getLogger()) {
}

Connectable::Connectable(const Connectable &&other)
    : CoreComponent(std::move(other)),
      max_concurrent_tasks_(std::move(other.max_concurrent_tasks_)),
      logger_(other.logger_) {
  has_work_ = other.has_work_.load();
  strategy_ = other.strategy_.load();
}

Connectable::~Connectable() {
}

bool Connectable::setSupportedRelationships(
    std::set<core::Relationship> relationships) {
  if (isRunning()) {
    logger_->log_info(
        "Can not set processor supported relationship while the process %s is running",
        name_.c_str());
    return false;
  }

  std::lock_guard<std::mutex> lock(relationship_mutex_);

  relationships_.clear();
  for (auto item : relationships) {
    relationships_[item.getName()] = item;
    logger_->log_info("Processor %s supported relationship name %s",
                      name_.c_str(), item.getName().c_str());
  }
  return true;
}

// Whether the relationship is supported
bool Connectable::isSupportedRelationship(core::Relationship relationship) {
  const bool requiresLock = isRunning();

  const auto conditionalLock =
      !requiresLock ?
          std::unique_lock<std::mutex>() :
          std::unique_lock<std::mutex>(relationship_mutex_);

  const auto &it = relationships_.find(relationship.getName());
  if (it != relationships_.end()) {
    return true;
  } else {
    return false;
  }
}

bool Connectable::setAutoTerminatedRelationships(
    std::set<Relationship> relationships) {
  if (isRunning()) {
    logger_->log_info(
        "Can not set processor auto terminated relationship while the process %s is running",
        name_.c_str());
    return false;
  }

  std::lock_guard<std::mutex> lock(relationship_mutex_);

  auto_terminated_relationships_.clear();
  for (auto item : relationships) {
    auto_terminated_relationships_[item.getName()] = item;
    logger_->log_info("Processor %s auto terminated relationship name %s",
                      name_.c_str(), item.getName().c_str());
  }
  return true;
}

// Check whether the relationship is auto terminated
bool Connectable::isAutoTerminated(core::Relationship relationship) {
  const bool requiresLock = isRunning();

  const auto conditionalLock =
      !requiresLock ?
          std::unique_lock<std::mutex>() :
          std::unique_lock<std::mutex>(relationship_mutex_);

  const auto &it = auto_terminated_relationships_.find(relationship.getName());
  if (it != auto_terminated_relationships_.end()) {
    return true;
  } else {
    return false;
  }
}

void Connectable::waitForWork(uint64_t timeoutMs) {
  has_work_.store(isWorkAvailable());

  if (!has_work_.load()) {
    std::unique_lock<std::mutex> lock(work_available_mutex_);
    work_condition_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [&] {return has_work_.load();});
  }
}

void Connectable::notifyWork() {
  // Do nothing if we are not event-driven
  if (strategy_ != EVENT_DRIVEN) {
    return;
  }

  {
    has_work_.store(isWorkAvailable());

    if (has_work_.load()) {
      work_condition_.notify_one();
    }
  }
}

std::set<std::shared_ptr<Connectable>> Connectable::getOutGoingConnections(
    std::string relationship) {
  std::set<std::shared_ptr<Connectable>> empty;

  auto &&it = out_going_connections_.find(relationship);
  if (it != out_going_connections_.end()) {
    return out_going_connections_[relationship];
  } else {
    return empty;
  }
}

std::shared_ptr<Connectable> Connectable::getNextIncomingConnection() {
  std::lock_guard<std::mutex> lock(relationship_mutex_);

  if (_incomingConnections.size() == 0)
    return NULL;

  if (incoming_connections_Iter == _incomingConnections.end())
    incoming_connections_Iter = _incomingConnections.begin();

  std::shared_ptr<Connectable> ret = *incoming_connections_Iter;
  incoming_connections_Iter++;

  if (incoming_connections_Iter == _incomingConnections.end())
    incoming_connections_Iter = _incomingConnections.begin();

  return ret;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
