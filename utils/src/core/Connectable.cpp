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
#include <utility>
#include <memory>
#include <string>
#include <set>
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core {

ConnectableImpl::ConnectableImpl(std::string_view name, const utils::Identifier &uuid)
    : CoreComponentImpl(name, uuid),
      max_concurrent_tasks_(1),
      connectable_version_(nullptr),
      logger_(logging::LoggerFactory<Connectable>::getLogger(uuid_)) {
}

ConnectableImpl::ConnectableImpl(std::string_view name)
    : CoreComponentImpl(name),
      max_concurrent_tasks_(1),
      connectable_version_(nullptr),
      logger_(logging::LoggerFactory<Connectable>::getLogger(uuid_)) {
}

ConnectableImpl::~ConnectableImpl() = default;

void ConnectableImpl::setSupportedRelationships(std::span<const core::RelationshipDefinition> relationships) {
  if (isRunning()) {
    logger_->log_warn("Cannot set processor supported relationship while the process {} is running", name_);
    return;
  }

  std::lock_guard<std::mutex> lock(relationship_mutex_);

  relationships_.clear();
  for (const auto& item : relationships) {
    relationships_.emplace(item.name, item);
    logger_->log_debug("Processor {} supported relationship name {}", name_, item.name);
  }
}

std::vector<Relationship> ConnectableImpl::getSupportedRelationships() const {
  std::vector<Relationship> relationships;
  relationships.reserve(relationships_.size());
  for (auto const &item : relationships_) {
    relationships.push_back(item.second);
  }
  return relationships;
}

bool ConnectableImpl::isSupportedRelationship(const core::Relationship &relationship) {
  // if we are running we do not need a lock since the function to change relationships_ ( setSupportedRelationships)
  // cannot be executed while we are running
  const bool isConnectableRunning = isRunning();

  const auto conditionalLock = isConnectableRunning ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(relationship_mutex_);

  return relationships_.contains(relationship.getName());
}

void ConnectableImpl::addAutoTerminatedRelationship(const core::Relationship& relationship) {
  if (isRunning()) {
    logger_->log_warn("Can not add processor auto terminated relationship while the process {} is running", name_);
    return;
  }

  std::lock_guard<std::mutex> lock(relationship_mutex_);

  auto_terminated_relationships_[relationship.getName()] = relationship;
}

void ConnectableImpl::setAutoTerminatedRelationships(std::span<const core::Relationship> relationships) {
  if (isRunning()) {
    logger_->log_warn("Can not set processor auto terminated relationship while the process {} is running", name_);
    return;
  }

  std::lock_guard<std::mutex> lock(relationship_mutex_);

  auto_terminated_relationships_.clear();
  for (const auto& item : relationships) {
    auto_terminated_relationships_[item.getName()] = item;
    logger_->log_debug("Processor {} auto terminated relationship name {}", name_, item.getName());
  }
}

bool ConnectableImpl::isAutoTerminated(const core::Relationship &relationship) {
  // if we are running we do not need a lock since the function to change relationships_ ( setSupportedRelationships)
  // cannot be executed while we are running
  const bool isConnectableRunning = isRunning();

  const auto conditionalLock = isConnectableRunning ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(relationship_mutex_);

  return auto_terminated_relationships_.contains(relationship.getName());
}

void ConnectableImpl::waitForWork(std::chrono::milliseconds timeout) {
  has_work_.store(isWorkAvailable());

  if (!has_work_.load()) {
    std::unique_lock<std::mutex> lock(work_available_mutex_);
    work_condition_.wait_for(lock, timeout, [&] {return has_work_.load();});
  }
}

void ConnectableImpl::notifyWork() {
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

std::set<Connectable*> ConnectableImpl::getOutGoingConnections(const std::string &relationship) {
  const auto it = outgoing_connections_.find(relationship);
  if (it != outgoing_connections_.end()) {
    return it->second;
  } else {
    return {};
  }
}

Connectable* ConnectableImpl::getNextIncomingConnection() {
  std::lock_guard<std::mutex> lock(relationship_mutex_);
  return getNextIncomingConnectionImpl(lock);
}

Connectable* ConnectableImpl::getNextIncomingConnectionImpl(const std::lock_guard<std::mutex>& /*relatioship_mutex_lock*/) {
  if (incoming_connections_.empty())
    return nullptr;

  if (incoming_connections_Iter == incoming_connections_.end())
    incoming_connections_Iter = incoming_connections_.begin();

  auto ret = *incoming_connections_Iter;
  incoming_connections_Iter++;

  if (incoming_connections_Iter == incoming_connections_.end())
    incoming_connections_Iter = incoming_connections_.begin();

  return ret;
}

Connectable* ConnectableImpl::pickIncomingConnection() {
  return getNextIncomingConnection();
}

}  // namespace org::apache::nifi::minifi::core
