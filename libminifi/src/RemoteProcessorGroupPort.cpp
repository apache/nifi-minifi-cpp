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
#include "RemoteProcessorGroupPort.h"
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <memory>
#include <sstream>
#include <iostream>
#include "../include/io/StreamFactory.h"
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
core::Property RemoteProcessorGroupPort::portUUID("Port UUID",
    "Specifies remote NiFi Port UUID.", "");
core::Relationship RemoteProcessorGroupPort::relation;

std::unique_ptr<Site2SiteClientProtocol> RemoteProcessorGroupPort::getNextProtocol() {
  std::lock_guard<std::mutex> protocol_lock_(protocol_mutex_);
  if (available_protocols_.empty())
    return nullptr;

  std::unique_ptr<Site2SiteClientProtocol> return_pointer = std::move(
      available_protocols_.top());
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
  properties.insert(portUUID);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(relation);
  setSupportedRelationships(relationships);
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

  std::shared_ptr<Site2SiteClientProtocol> protocol_ =
      this->obtainSite2SiteProtocol(stream_factory_, host, sport, protocol_uuid_);

  if (!protocol_) {
    context->yield();
    return;
  }

  if (!protocol_->bootstrap()) {
    // bootstrap the client protocol if needeed
    context->yield();
    std::shared_ptr<Processor> processor = std::static_pointer_cast < Processor
        > (context->getProcessorNode().getProcessor());
    logger_->log_error("Site2Site bootstrap failed yield period %d peer ",
        processor->getYieldPeriodMsec());
    returnSite2SiteProtocol(protocol_);
    return;
  }

  if (direction_ == RECEIVE)
    protocol_->receiveFlowFiles(context, session);
  else
    protocol_->transferFlowFiles(context, session);

  returnSite2SiteProtocol(protocol_);

  return;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
