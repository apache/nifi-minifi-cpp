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

#include "CoapConnector.h"

#include <string>
#include <memory>
#include <set>

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "io/validation.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace controllers {

static core::Property RemoteServer;
static core::Property Port;
static core::Property MaxQueueSize;

core::Property CoapConnectorService::RemoteServer(core::PropertyBuilder::createProperty("Remote Server")->withDescription("Remote CoAP server")->isRequired(false)->build());
core::Property CoapConnectorService::Port(
    core::PropertyBuilder::createProperty("Remote Port")->withDescription("Remote CoAP server port")->withDefaultValue<uint64_t>(8181)->isRequired(true)->build());
core::Property CoapConnectorService::MaxQueueSize(
    core::PropertyBuilder::createProperty("Max Queue Size")->withDescription("Max queue size for received data ")->withDefaultValue<uint64_t>(1000)->isRequired(false)->build());

void CoapConnectorService::initialize() {
  if (initialized_)
    return;

  CoapMessaging::getInstance();

  std::lock_guard<std::mutex> lock(initialization_mutex_);

  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

void CoapConnectorService::onEnable() {
  std::string port_str;
  if (getProperty(RemoteServer.getName(), host_) && !host_.empty() && getProperty(Port.getName(), port_str) && !port_str.empty()) {
    core::Property::StringToInt(port_str, port_);
  } else {
    // this is the case where we aren't being used in the context of a single controller service.
    if (configuration_->get("nifi.c2.agent.coap.host", host_) && configuration_->get("nifi.c2.agent.coap.port", port_str)) {
      core::Property::StringToInt(port_str, port_);
    }
  }
}

CoapResponse CoapConnectorService::sendPayload(uint8_t type, const std::string &endpoint, const CoapMessage *message) {
  // internally we are dealing with CoAPMessage in the two way communication, but the C++ ControllerService
  // will provide a CoAPResponse
  auto pdu = create_connection(type, host_.c_str(), endpoint.c_str(), port_, message);
  send_pdu(pdu);
  auto response = CoapMessaging::getInstance().pop(pdu->ctx);
  free_pdu(pdu);
  return response;
}

void CoapConnectorService::initializeProperties() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(RemoteServer);
  supportedProperties.insert(Port);
  supportedProperties.insert(MaxQueueSize);
  setSupportedProperties(supportedProperties);
}

REGISTER_INTERNAL_RESOURCE(CoapConnectorService);

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
