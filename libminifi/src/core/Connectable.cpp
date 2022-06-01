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
#include "core/logging/LoggerConfiguration.h"
#include "core/Relationship.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

Connectable::Connectable(const std::string &name, const utils::Identifier &uuid)
    : CoreComponent(name, uuid),
      max_concurrent_tasks_(1),
      connectable_version_(nullptr),
      logger_(logging::LoggerFactory<Connectable>::getLogger()) {
}

Connectable::Connectable(const std::string &name)
    : CoreComponent(name),
      max_concurrent_tasks_(1),
      connectable_version_(nullptr),
      logger_(logging::LoggerFactory<Connectable>::getLogger()) {
}

Connectable::~Connectable() = default;

std::vector<Relationship> Connectable::getSupportedRelationships() const {
  std::vector<Relationship> relationships;
  for (auto const &item : relationships_) {
    relationships.push_back(item.second);
  }
  return relationships;
}

// Whether the relationship is supported
bool Connectable::isSupportedRelationship(const core::Relationship &relationship) {
  // if we are running we do not need a lock since the function to change relationships_ ( setSupportedRelationships)
  // cannot be executed while we are running
  const bool isConnectableRunning = isRunning();

  const auto conditionalLock = isConnectableRunning ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(relationship_mutex_);

  const auto &it = relationships_.find(relationship.getName());
  if (it != relationships_.end()) {
    return true;
  } else {
    return false;
  }
}

bool Connectable::isAutoTerminated(const core::Relationship &relationship) {
  // if we are running we do not need a lock since the function to change relationships_ ( setSupportedRelationships)
  // cannot be executed while we are running
  const bool isConnectableRunning = isRunning();

  const auto conditionalLock = isConnectableRunning ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(relationship_mutex_);

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
    work_condition_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {return has_work_.load();});
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

std::set<Connectable*> Connectable::getOutGoingConnections(const std::string &relationship) {
  const auto it = outgoing_connections_.find(relationship);
  if (it != outgoing_connections_.end()) {
    return it->second;
  } else {
    return {};
  }
}

Connectable* Connectable::getNextIncomingConnection() {
  std::lock_guard<std::mutex> lock(relationship_mutex_);
  return getNextIncomingConnectionImpl(lock);
}

Connectable* Connectable::getNextIncomingConnectionImpl(const std::lock_guard<std::mutex>& /*relatioship_mutex_lock*/) {
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

Connectable* Connectable::pickIncomingConnection() {
  return getNextIncomingConnection();
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
