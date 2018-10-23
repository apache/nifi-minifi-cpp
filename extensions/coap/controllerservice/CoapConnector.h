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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_COAPCONNECTOR_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_COAPCONNECTOR_H_

#include "core/logging/LoggerConfiguration.h"
#include "core/controller/ControllerService.h"

#include "CoapResponse.h"
#include "CoapMessaging.h"
#include "coap_functions.h"
#include "coap_connection.h"
#include "coap_message.h"
#include <memory>
#include <unordered_map>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace controllers {

/**
 * Purpose and Justification: Controller services function as a layerable way to provide
 * services to internal services. While a controller service is generally configured from the flow,
 * we want to follow the open closed principle and provide CoAP services to other components.
 */
class CoapConnectorService : public core::controller::ControllerService {
 public:

  /**
   * Constructors for the controller service.
   */
  explicit CoapConnectorService(const std::string &name, const std::string &id)
      : ControllerService(name, id),
        port_(0),
        initialized_(false),
        logger_(logging::LoggerFactory<CoapConnectorService>::getLogger()) {
    initialize();
  }

  explicit CoapConnectorService(const std::string &name, utils::Identifier uuid = utils::Identifier())
      : ControllerService(name, uuid),
        port_(0),
        initialized_(false),
        logger_(logging::LoggerFactory<CoapConnectorService>::getLogger()) {
    initialize();
  }

  explicit CoapConnectorService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name),
        port_(0),
        initialized_(false),
        logger_(logging::LoggerFactory<CoapConnectorService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
  }

  /**
   * Parameters needed.
   */
  static core::Property RemoteServer;
  static core::Property Port;
  static core::Property MaxQueueSize;

  virtual void initialize();

  void yield() {

  }

  bool isRunning() {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() {
    return false;
  }

  virtual void onEnable();

  /**
   * Sends the payload to the endpoint, returning the response as we await. Will retry transmission
   * @param type type of payload to endpoint interaction ( GET, POST, PUT, DELETE ).
   * @param end endpoint is the connecting endpoint on the server
   * @param payload is the data to be sent
   * @param size size of the payload to be sent
   * @return CoAPMessage that contains the response code and data, if any.
   */
  CoapResponse sendPayload(uint8_t type, const std::string &endpoint, const CoapMessage * const message);

 protected:

  void initializeProperties();

  // connector mutex to controll access to the mapping, above.
  std::mutex connector_mutex_;

  // initialization mutex.
  std::mutex initialization_mutex_;

  std::atomic<bool> initialized_;

 private:

  // host connecting to.
  std::string host_;
  // port connecting to
  unsigned int port_;

  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace controllers */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_COAPCONNECTOR_H_ */
