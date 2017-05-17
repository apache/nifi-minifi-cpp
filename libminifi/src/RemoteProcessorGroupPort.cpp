/**
 * @file RemoteProcessorGroupPort.cpp
 * RemoteProcessorGroupPort class implementation
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

#include "../include/RemoteProcessorGroupPort.h"

#include <uuid/uuid.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <deque>
#include <iostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

#include "../include/core/logging/Logger.h"
#include "../include/core/ProcessContext.h"
#include "../include/core/ProcessorNode.h"
#include "../include/core/Property.h"
#include "../include/core/Relationship.h"
#include "../include/Site2SitePeer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

const std::string RemoteProcessorGroupPort::ProcessorName(
    "RemoteProcessorGroupPort");
core::Property RemoteProcessorGroupPort::hostName("Host Name",
                                                  "Remote Host Name.",
                                                  "localhost");
core::Property RemoteProcessorGroupPort::port("Port", "Remote Port", "9999");
core::Property RemoteProcessorGroupPort::portUUID(
    "Port UUID", "Specifies remote NiFi Port UUID.", "");
core::Relationship RemoteProcessorGroupPort::relation;

std::unique_ptr<Site2SiteClientProtocol> RemoteProcessorGroupPort::getNextProtocol(
    bool create = true) {
  std::unique_ptr<Site2SiteClientProtocol> nextProtocol = nullptr;
  if (!available_protocols_.try_dequeue(nextProtocol)) {
    if (create) {
      // create
      nextProtocol = std::unique_ptr<Site2SiteClientProtocol>(
          new Site2SiteClientProtocol(nullptr));
      nextProtocol->setPortId(protocol_uuid_);
      std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str =
          std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(
              stream_factory_->createSocket(host_, port_));
      std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(
          new Site2SitePeer(std::move(str), host_, port_));
      nextProtocol->setPeer(std::move(peer_));
    }
  }
  return std::move(nextProtocol);
}

void RemoteProcessorGroupPort::returnProtocol(
    std::unique_ptr<Site2SiteClientProtocol> return_protocol) {

  if (available_protocols_.size_approx() >= max_concurrent_tasks_) {
    // let the memory be freed
    return;
  }
  available_protocols_.enqueue(std::move(return_protocol));
}

void RemoteProcessorGroupPort::initialize() {
// Set the supported properties
  std::set<core::Property> properties;
  properties.insert(hostName);
  properties.insert(port);
  properties.insert(portUUID);
  setSupportedProperties(properties);
// Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(relation);
  setSupportedRelationships(relationships);
}

void RemoteProcessorGroupPort::onSchedule(
    core::ProcessContext *context,
    core::ProcessSessionFactory *sessionFactory) {
  std::string value;

  int64_t lvalue;

  if (context->getProperty(hostName.getName(), value)) {
    host_ = value;
  }
  if (context->getProperty(port.getName(), value)
      && core::Property::StringToInt(value, lvalue)) {
    port_ = (uint16_t) lvalue;
  }
  if (context->getProperty(portUUID.getName(), value)) {
    uuid_parse(value.c_str(), protocol_uuid_);
  }
}

void RemoteProcessorGroupPort::onTrigger(core::ProcessContext *context,
                                         core::ProcessSession *session) {
  if (!transmitting_)
    return;

  std::string value;

  int64_t lvalue;

  std::string host = "";
  uint16_t sport = 0;

  if (context->getProperty(hostName.getName(), value)) {
    host = value;
  }
  if (context->getProperty(port.getName(), value)
      && core::Property::StringToInt(value, lvalue)) {
    sport = (uint16_t) lvalue;
  }
  if (context->getProperty(portUUID.getName(), value)) {
    uuid_parse(value.c_str(), protocol_uuid_);
  }

  std::unique_ptr<Site2SiteClientProtocol> protocol_ = getNextProtocol();

  if (!protocol_) {
    context->yield();
    return;
  }

  if (!protocol_->bootstrap()) {
    // bootstrap the client protocol if needeed
    context->yield();
    std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(
        context->getProcessorNode().getProcessor());
    logger_->log_error("Site2Site bootstrap failed yield period %d peer ",
                       processor->getYieldPeriodMsec());
    returnProtocol(std::move(protocol_));
    return;
  }

  if (direction_ == RECEIVE) {
    protocol_->receiveFlowFiles(context, session);
  } else {
    protocol_->transferFlowFiles(context, session);
  }

  returnProtocol(std::move(protocol_));

  return;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
