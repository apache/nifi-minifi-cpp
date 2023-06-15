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
#pragma once

#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <utility>

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"

#include "CoapResponse.h"
#include "CoapMessaging.h"
#include "coap_functions.h"
#include "coap_connection.h"
#include "coap_message.h"

namespace org::apache::nifi::minifi::coap::controllers {

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide CoAP services to other components.
 */
class CoapConnectorService : public core::controller::ControllerService {
 public:
  explicit CoapConnectorService(std::string name, const utils::Identifier &uuid = {})
      : ControllerService(std::move(name), uuid) {
    initialize();
  }

  explicit CoapConnectorService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name) {
    setConfiguration(configuration);
    initialize();
  }

  EXTENSIONAPI static constexpr auto RemoteServer = core::PropertyDefinitionBuilder<>::createProperty("Remote Server")
      .withDescription("Remote CoAP server")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto Port = core::PropertyDefinitionBuilder<>::createProperty("Remote Port")
      .withDescription("Remote CoAP server port")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("8181")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxQueueSize = core::PropertyDefinitionBuilder<>::createProperty("Max Queue Size")
      .withDescription("Max queue size for received data ")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("1000")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 3>{
      RemoteServer,
      Port,
      MaxQueueSize
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;

  void yield() override { }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void onEnable() override;

  /**
   * Sends the payload to the endpoint, returning the response as we await. Will retry transmission
   * @param type type of payload to endpoint interaction ( GET, POST, PUT, DELETE ).
   * @param end endpoint is the connecting endpoint on the server
   * @param payload is the data to be sent
   * @param size size of the payload to be sent
   * @return CoAPMessage that contains the response code and data, if any.
   */
  CoapResponse sendPayload(uint8_t type, const std::string &endpoint, const CoapMessage * message);

 protected:
  void initializeProperties();

  // connector mutex to controll access to the mapping, above.
  std::mutex connector_mutex_;

  // initialization mutex.
  std::mutex initialization_mutex_;

  bool initialized_ = false;

 private:
  // host connecting to.
  std::string host_;
  // port connecting to
  unsigned int port_{ 0 };

  std::shared_ptr<core::logging::Logger> logger_{ core::logging::LoggerFactory<CoapConnectorService>::getLogger() };
};

}  // namespace org::apache::nifi::minifi::coap::controllers
