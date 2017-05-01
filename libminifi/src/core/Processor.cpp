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
#include <sys/time.h>
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
#include "core/Connectable.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "../include/io/StreamFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

Processor::Processor(std::string name, uuid_t uuid)
    : Connectable(name, uuid),
      ConfigurableComponent(logging::Logger::getLogger()) {
  has_work_.store(false);
  // Setup the default values
  state_ = DISABLED;
  strategy_ = TIMER_DRIVEN;
  loss_tolerant_ = false;
  _triggerWhenEmpty = false;
  protocols_created_ = false;
  scheduling_period_nano_ = MINIMUM_SCHEDULING_NANOS;
  run_durantion_nano_ = 0;
  yield_period_msec_ = DEFAULT_YIELD_PERIOD_SECONDS * 1000;
  _penalizationPeriodMsec = DEFAULT_PENALIZATION_PERIOD_SECONDS * 1000;
  max_concurrent_tasks_ = 1;
  active_tasks_ = 0;
  yield_expiration_ = 0;
  incoming_connections_Iter = this->_incomingConnections.begin();
  logger_ = logging::Logger::getLogger();
  logger_->log_info("Processor %s created UUID %s", name_.c_str(),
                    uuidStr_.c_str());
}

bool Processor::isRunning() {
  return (state_ == RUNNING && active_tasks_ > 0);
}

void Processor::setScheduledState(ScheduledState state) {
  state_ = state;
}

bool Processor::addConnection(std::shared_ptr<Connectable> conn) {
  bool ret = false;

  if (isRunning()) {
    logger_->log_info("Can not add connection while the process %s is running",
                      name_.c_str());
    return false;
  }
  std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(
      conn);
  std::lock_guard<std::mutex> lock(mutex_);

  uuid_t srcUUID;
  uuid_t destUUID;

  connection->getSourceUUID(srcUUID);
  connection->getDestinationUUID(destUUID);
  char uuid_str[37];

  uuid_unparse_lower(uuid_, uuid_str);
  std::string my_uuid = uuid_str;
  uuid_unparse_lower(destUUID, uuid_str);
  std::string destination_uuid = uuid_str;
  if (my_uuid == destination_uuid) {
    // Connection is destination to the current processor
    if (_incomingConnections.find(connection) == _incomingConnections.end()) {
      _incomingConnections.insert(connection);
      connection->setDestination(shared_from_this());
      logger_->log_info(
          "Add connection %s into Processor %s incoming connection",
          connection->getName().c_str(), name_.c_str());
      incoming_connections_Iter = this->_incomingConnections.begin();
      ret = true;
    }
  }
  uuid_unparse_lower(srcUUID, uuid_str);
  std::string source_uuid = uuid_str;
  if (my_uuid == source_uuid) {
    std::string relationship = connection->getRelationship().getName();
    // Connection is source from the current processor
    auto &&it = _outGoingConnections.find(relationship);
    if (it != _outGoingConnections.end()) {
      // We already has connection for this relationship
      std::set<std::shared_ptr<Connectable>> existedConnection = it->second;
      if (existedConnection.find(connection) == existedConnection.end()) {
        // We do not have the same connection for this relationship yet
        existedConnection.insert(connection);
        connection->setSource(shared_from_this());
        _outGoingConnections[relationship] = existedConnection;
        logger_->log_info(
            "Add connection %s into Processor %s outgoing connection for relationship %s",
            connection->getName().c_str(), name_.c_str(), relationship.c_str());
        ret = true;
      }
    } else {
      // We do not have any outgoing connection for this relationship yet
      std::set<std::shared_ptr<Connectable>> newConnection;
      newConnection.insert(connection);
      connection->setSource(shared_from_this());
      _outGoingConnections[relationship] = newConnection;
      logger_->log_info(
          "Add connection %s into Processor %s outgoing connection for relationship %s",
          connection->getName().c_str(), name_.c_str(), relationship.c_str());
      ret = true;
    }
  }

  return ret;
}

void Processor::removeConnection(std::shared_ptr<Connectable> conn) {
  if (isRunning()) {
    logger_->log_info(
        "Can not remove connection while the process %s is running",
        name_.c_str());
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  uuid_t srcUUID;
  uuid_t destUUID;

  std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(
      conn);

  connection->getSourceUUID(srcUUID);
  connection->getDestinationUUID(destUUID);

  if (uuid_compare(uuid_, destUUID) == 0) {
    // Connection is destination to the current processor
    if (_incomingConnections.find(connection) != _incomingConnections.end()) {
      _incomingConnections.erase(connection);
      connection->setDestination(NULL);
      logger_->log_info(
          "Remove connection %s into Processor %s incoming connection",
          connection->getName().c_str(), name_.c_str());
      incoming_connections_Iter = this->_incomingConnections.begin();
    }
  }

  if (uuid_compare(uuid_, srcUUID) == 0) {
    std::string relationship = connection->getRelationship().getName();
    // Connection is source from the current processor
    auto &&it = _outGoingConnections.find(relationship);
    if (it == _outGoingConnections.end()) {
      return;
    } else {
      if (_outGoingConnections[relationship].find(connection)
          != _outGoingConnections[relationship].end()) {
        _outGoingConnections[relationship].erase(connection);
        connection->setSource(NULL);
        logger_->log_info(
            "Remove connection %s into Processor %s outgoing connection for relationship %s",
            connection->getName().c_str(), name_.c_str(), relationship.c_str());
      }
    }
  }
}

std::shared_ptr<Site2SiteClientProtocol> Processor::obtainSite2SiteProtocol(
    std::shared_ptr<io::StreamFactory> stream_factory, std::string host, uint16_t sport, uuid_t portId) {
  std::lock_guard < std::mutex > lock(mutex_);

  if (!protocols_created_) {
    for (int i = 0; i < this->max_concurrent_tasks_; i++) {
      // create the protocol pool based on max threads allowed
      std::shared_ptr<Site2SiteClientProtocol> protocol = std::make_shared<Site2SiteClientProtocol>(nullptr);
      protocols_created_ = true;
      protocol->setPortId(portId);
      std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str =
          std::unique_ptr < org::apache::nifi::minifi::io::DataStream
              > (stream_factory->createSocket(host, sport));
      std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr < Site2SitePeer
          > (new Site2SitePeer(std::move(str), host, sport));
      protocol->setPeer(std::move(peer_));
      available_protocols_.push(protocol);
    }
  }
  if (!available_protocols_.empty()) {
    std::shared_ptr<Site2SiteClientProtocol> return_pointer =
        available_protocols_.top();
    available_protocols_.pop();
    return return_pointer;
  } else {
    // create the protocol on demand if we exceed the pool
    std::shared_ptr<Site2SiteClientProtocol> protocol = std::make_shared<Site2SiteClientProtocol>(nullptr);
    protocol->setPortId(portId);
    std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str =
        std::unique_ptr < org::apache::nifi::minifi::io::DataStream
            > (stream_factory->createSocket(host, sport));
    std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr < Site2SitePeer
        > (new Site2SitePeer(std::move(str), host, sport));
    protocol->setPeer(std::move(peer_));
    return protocol;
  }
}

void Processor::returnSite2SiteProtocol(
    std::shared_ptr<Site2SiteClientProtocol> protocol) {
  std::lock_guard < std::mutex > lock(mutex_);
  if (protocol && available_protocols_.size() < max_concurrent_tasks_) {
    available_protocols_.push(protocol);
  }
}

bool Processor::flowFilesQueued() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (_incomingConnections.size() == 0)
    return false;

  for (auto &&conn : _incomingConnections) {
    std::shared_ptr<Connection> connection =
        std::static_pointer_cast<Connection>(conn);
    if (connection->getQueueSize() > 0)
      return true;
  }

  return false;
}

bool Processor::flowFilesOutGoingFull() {
  std::lock_guard<std::mutex> lock(mutex_);

  for (auto &&connection : _outGoingConnections) {
    // We already has connection for this relationship
    std::set<std::shared_ptr<Connectable>> existedConnection = connection.second;
    for (const auto conn : existedConnection) {
      std::shared_ptr<Connection> connection = std::static_pointer_cast<
          Connection>(conn);
      if (connection->isFull())
        return true;
    }
  }

  return false;
}

void Processor::onTrigger(ProcessContext *context,
                          ProcessSessionFactory *sessionFactory) {
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

bool Processor::isWorkAvailable() {
  // We have work if any incoming connection has work
  bool hasWork = false;

  try {
    for (const auto &conn : _incomingConnections) {
      std::shared_ptr<Connection> connection = std::static_pointer_cast<
          Connection>(conn);
      if (connection->getQueueSize() > 0) {
        hasWork = true;
        break;
      }
    }
  } catch (...) {
    logger_->log_error(
        "Caught an exception while checking if work is available;"
        " unless it was positively determined that work is available, assuming NO work is available!");
  }

  return hasWork;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
