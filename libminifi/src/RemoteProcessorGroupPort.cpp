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
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <sys/time.h>
#include <time.h>
#include <sstream>
#include <string.h>
#include <iostream>

#include "RemoteProcessorGroupPort.h"

#include "../include/io/StreamFactoryh"
#include "io/ClientSocket.h"
#include "utils/TimeUtil.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

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
core::Relationship RemoteProcessorGroupPort::relation;


std::unique_ptr<Site2SiteClientProtocol> RemoteProcessorGroupPort::getNextProtocol() {
  std::lock_guard<std::mutex> protocol_lock_(protocol_mutex_);
  if (available_protocols_.empty())
    return nullptr;

  std::unique_ptr<Site2SiteClientProtocol> return_pointer = std::move(available_protocols_.top());
  available_protocols_.pop();
  return std::move(return_pointer);
}

void RemoteProcessorGroupPort::returnProtocol(
    std::unique_ptr<Site2SiteClientProtocol> return_protocol) {
  std::lock_guard<std::mutex> protocol_lock_(protocol_mutex_);
  available_protocols_.push(std::move(return_protocol));
}

void RemoteProcessorGroupPort::initialize() {

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(hostName);
  properties.insert(port);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(relation);
  setSupportedRelationships(relationships);

}

void RemoteProcessorGroupPort::onTrigger(core::ProcessContext *context,
                                         core::ProcessSession *session) {
  std::string value;

  if (!transmitting_)
    return;

  std::unique_ptr<Site2SiteClientProtocol> protocol_ = getNextProtocol();

  // Peer Connection
  if (protocol_ == nullptr) {

    protocol_ = std::unique_ptr<Site2SiteClientProtocol>(
        new Site2SiteClientProtocol(0));
    protocol_->setPortId(protocol_uuid_);
    protocol_->setTimeOut(timeout_);

    std::string host = "";
    uint16_t sport = 0;
    int64_t lvalue;

    if (context->getProperty(hostName.getName(), value)) {
      host = value;
    }
    if (context->getProperty(port.getName(), value)
        && core::Property::StringToInt(value, lvalue)) {
      sport = (uint16_t) lvalue;
    }
    std::unique_ptr<org::apache::nifi::minifi::io::DataStream> str =
        std::unique_ptr<org::apache::nifi::minifi::io::DataStream>(
            org::apache::nifi::minifi::io::StreamFactory::getInstance()
                ->createSocket(host, sport));

    std::unique_ptr<Site2SitePeer> peer_ = std::unique_ptr<Site2SitePeer>(
        new Site2SitePeer(std::move(str), host, sport));

    protocol_->setPeer(std::move(peer_));
  }

  if (!protocol_->bootstrap()) {
    // bootstrap the client protocol if needeed
    context->yield();
    std::shared_ptr<Processor> processor = std::static_pointer_cast<Processor>(
        context->getProcessorNode().getProcessor());
    logger_->log_error("Site2Site bootstrap failed yield period %d peer ",
                       processor->getYieldPeriodMsec());
    return;
  }

  if (direction_ == RECEIVE)
    protocol_->receiveFlowFiles(context, session);
  else
    protocol_->transferFlowFiles(context, session);

  returnProtocol(std::move(protocol_));

  return;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
