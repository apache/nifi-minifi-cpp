/**
 * @file Processor.cpp
 * Processor class implementation
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
#include "core/Processor.h"
#include <time.h>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <chrono>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include <utility>
#include "Connection.h"
#include "core/ProcessorConfig.h"
#include "core/Connectable.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "io/StreamFactory.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

Processor::Processor(std::string name)
    : Connectable(name),
      ConfigurableComponent(),
      logger_(logging::LoggerFactory<Processor>::getLogger()) {
  has_work_.store(false);
  // Setup the default values
  state_ = DISABLED;
  strategy_ = TIMER_DRIVEN;
  loss_tolerant_ = false;
  _triggerWhenEmpty = false;
  scheduling_period_nano_ = MINIMUM_SCHEDULING_NANOS;
  run_duration_nano_ = DEFAULT_RUN_DURATION;
  yield_period_msec_ = DEFAULT_YIELD_PERIOD_SECONDS * 1000;
  _penalizationPeriodMsec = DEFAULT_PENALIZATION_PERIOD_SECONDS * 1000;
  max_concurrent_tasks_ = DEFAULT_MAX_CONCURRENT_TASKS;
  active_tasks_ = 0;
  yield_expiration_ = 0;
  incoming_connections_Iter = this->_incomingConnections.begin();
  logger_->log_debug("Processor %s created UUID %s", name_, uuidStr_);
}

Processor::Processor(std::string name, utils::Identifier &uuid)
    : Connectable(name, uuid),
      ConfigurableComponent(),
      logger_(logging::LoggerFactory<Processor>::getLogger()) {
  has_work_.store(false);
  // Setup the default values
  state_ = DISABLED;
  strategy_ = TIMER_DRIVEN;
  loss_tolerant_ = false;
  _triggerWhenEmpty = false;
  scheduling_period_nano_ = MINIMUM_SCHEDULING_NANOS;
  run_duration_nano_ = DEFAULT_RUN_DURATION;
  yield_period_msec_ = DEFAULT_YIELD_PERIOD_SECONDS * 1000;
  _penalizationPeriodMsec = DEFAULT_PENALIZATION_PERIOD_SECONDS * 1000;
  max_concurrent_tasks_ = DEFAULT_MAX_CONCURRENT_TASKS;
  active_tasks_ = 0;
  yield_expiration_ = 0;
  incoming_connections_Iter = this->_incomingConnections.begin();
  logger_->log_debug("Processor %s created UUID %s with uuid %s", name_, uuidStr_, uuid.to_string());
}

bool Processor::isRunning() {
  return (state_ == RUNNING && active_tasks_ > 0);
}

void Processor::setScheduledState(ScheduledState state) {
  state_ = state;
  if (state == STOPPED) {
    notifyStop();
  }
}

bool Processor::addConnection(std::shared_ptr<Connectable> conn) {
  bool ret = false;

  if (isRunning()) {
    logger_->log_warn("Can not add connection while the process %s is running", name_);
    return false;
  }
  std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
  std::lock_guard<std::mutex> lock(mutex_);

  utils::Identifier srcUUID;
  utils::Identifier destUUID;

  connection->getSourceUUID(srcUUID);
  connection->getDestinationUUID(destUUID);
  std::string my_uuid = uuid_.to_string();
  std::string destination_uuid = destUUID.to_string();
  if (my_uuid == destination_uuid) {
    // Connection is destination to the current processor
    if (_incomingConnections.find(connection) == _incomingConnections.end()) {
      _incomingConnections.insert(connection);
      connection->setDestination(shared_from_this());
      logger_->log_debug("Add connection %s into Processor %s incoming connection", connection->getName(), name_);
      incoming_connections_Iter = this->_incomingConnections.begin();
      ret = true;
    }
  }
  std::string source_uuid = srcUUID.to_string();
  if (my_uuid == source_uuid) {
    const auto &rels = connection->getRelationships();
    for (auto i = rels.begin(); i != rels.end(); i++) {
      const auto relationship = (*i).getName();
      // Connection is source from the current processor
      auto &&it = out_going_connections_.find(relationship);
      if (it != out_going_connections_.end()) {
        // We already has connection for this relationship
        std::set<std::shared_ptr<Connectable>> existedConnection = it->second;
        if (existedConnection.find(connection) == existedConnection.end()) {
          // We do not have the same connection for this relationship yet
          existedConnection.insert(connection);
          connection->setSource(shared_from_this());
          out_going_connections_[relationship] = existedConnection;
          logger_->log_debug("Add connection %s into Processor %s outgoing connection for relationship %s", connection->getName(), name_, relationship);
          ret = true;
        }
      } else {
        // We do not have any outgoing connection for this relationship yet
        std::set<std::shared_ptr<Connectable>> newConnection;
        newConnection.insert(connection);
        connection->setSource(shared_from_this());
        out_going_connections_[relationship] = newConnection;
        logger_->log_debug("Add connection %s into Processor %s outgoing connection for relationship %s", connection->getName(), name_, relationship);
        ret = true;
      }
    }
  }
  return ret;
}

void Processor::removeConnection(std::shared_ptr<Connectable> conn) {
  if (isRunning()) {
    logger_->log_warn("Can not remove connection while the process %s is running", name_);
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  utils::Identifier srcUUID;
  utils::Identifier destUUID;

  std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);

  connection->getSourceUUID(srcUUID);
  connection->getDestinationUUID(destUUID);

  if (uuid_ == destUUID) {
    // Connection is destination to the current processor
    if (_incomingConnections.find(connection) != _incomingConnections.end()) {
      _incomingConnections.erase(connection);
      connection->setDestination(NULL);
      logger_->log_debug("Remove connection %s into Processor %s incoming connection", connection->getName(), name_);
      incoming_connections_Iter = this->_incomingConnections.begin();
    }
  }

  if (uuid_ == srcUUID) {
    const auto &rels = connection->getRelationships();
    for (auto i = rels.begin(); i != rels.end(); i++) {
      const auto relationship = (*i).getName();
      // Connection is source from the current processor
      auto &&it = out_going_connections_.find(relationship);
      if (it != out_going_connections_.end()) {
        if (out_going_connections_[relationship].find(connection) != out_going_connections_[relationship].end()) {
          out_going_connections_[relationship].erase(connection);
          connection->setSource(NULL);
          logger_->log_debug("Remove connection %s into Processor %s outgoing connection for relationship %s", connection->getName(), name_, relationship);
        }
      }
    }
  }
}

bool Processor::flowFilesQueued() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (_incomingConnections.size() == 0)
    return false;

  for (auto &&conn : _incomingConnections) {
    std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
    if (connection->getQueueSize() > 0)
      return true;
  }

  return false;
}

bool Processor::flowFilesOutGoingFull() {
  std::lock_guard<std::mutex> lock(mutex_);

  for (auto &&connection : out_going_connections_) {
    // We already has connection for this relationship
    std::set<std::shared_ptr<Connectable>> existedConnection = connection.second;
    for (const auto conn : existedConnection) {
      std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
      if (connection->isFull())
        return true;
    }
  }

  return false;
}

void Processor::onTrigger(ProcessContext *context, ProcessSessionFactory *sessionFactory) {
  auto session = sessionFactory->createSession();

  try {
    // Call the virtual trigger function
    onTrigger(context, session.get());
    session->commit();
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    session->rollback();
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception Processor::onTrigger");
    session->rollback();
    throw;
  }
}

void Processor::onTrigger(const std::shared_ptr<ProcessContext> &context, const std::shared_ptr<ProcessSessionFactory> &sessionFactory) {
  auto session = sessionFactory->createSession();

  try {
    // Call the virtual trigger function
    onTrigger(context, session);
    session->commit();
  } catch (std::exception &exception) {
    logger_->log_debug("Caught Exception %s", exception.what());
    session->rollback();
    throw;
  } catch (...) {
    logger_->log_debug("Caught Exception Processor::onTrigger");
    session->rollback();
    throw;
  }
}

bool Processor::isWorkAvailable() {
  // We have work if any incoming connection has work
  bool hasWork = false;

  try {
    for (const auto &conn : _incomingConnections) {
      std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
      if (connection->getQueueSize() > 0) {
        hasWork = true;
        break;
      }
    }
  } catch (...) {
    logger_->log_error("Caught an exception while checking if work is available;"
                       " unless it was positively determined that work is available, assuming NO work is available!");
  }

  return hasWork;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
