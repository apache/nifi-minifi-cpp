/**
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

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"
#include "core/Resource.h"
#include "io/validation.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::coap::controllers {

void CoapConnectorService::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_) {
    return;
  }

  CoapMessaging::getInstance();

  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

void CoapConnectorService::onEnable() {
  std::string port_str;
  if (getProperty(RemoteServer, host_) && !host_.empty() && getProperty(Port, port_str) && !port_str.empty()) {
    core::Property::StringToInt(port_str, port_);
  } else {
    // this is the case where we aren't being used in the context of a single controller service.
    if (configuration_->get(Configuration::nifi_c2_agent_coap_host, host_) && configuration_->get(Configuration::nifi_c2_agent_coap_port, port_str)) {
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
  setSupportedProperties(Properties);
}

REGISTER_RESOURCE(CoapConnectorService, InternalResource);

}  // namespace org::apache::nifi::minifi::coap::controllers
