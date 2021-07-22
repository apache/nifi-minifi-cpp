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
#pragma once

#include <cstring>
#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <mutex>

#include "c2/C2Protocol.h"
#include "io/BaseStream.h"
#include "agent/agent_version.h"
#include "CoapConnector.h"

#include "coap2/coap.h"
#include "coap2/uri.h"
#include "coap2/address.h"
#include "protocols/RESTSender.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace c2 {

#define REQUIRE_VALID(x) \
  if (io::isError(x)) { \
    return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR); \
  }

#define REQUIRE_SIZE_IF(y, x) \
  if (y != x) { \
    return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR); \
  }

/**
 * CoAP is the Constrained Application Protocol, which defines a specialized web transfer protocol that can be
 * used on devices with constrained resources.
 */
class CoapProtocol : public minifi::c2::RESTSender {
 public:
  explicit CoapProtocol(const std::string &name, const utils::Identifier &uuid = utils::Identifier());

  ~CoapProtocol() override;

  /**
   * Consume the payload.
   * @param url to evaluate.
   * @param payload payload to consume.
   * @param direction direction of operation.
   */
  minifi::c2::C2Payload consumePayload(const std::string &url, const minifi::c2::C2Payload &payload, minifi::c2::Direction direction, bool async) override;

  minifi::c2::C2Payload consumePayload(const minifi::c2::C2Payload &payload, minifi::c2::Direction /*direction*/, bool /*async*/) override {
      return serialize(payload);
  }

  void update(const std::shared_ptr<Configure>& /*configure*/) override {
    // no op.
  }

  void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) override;

  // Supported Properties

 protected:
  bool isRegistrationMessage(controllers::CoapResponse &response) {
    if (LIKELY(response.getSize() != 8)) {
      return false;
    }
    return response.getCode() == COAP_RESPONSE_400 && !memcmp(response.getData(), REGISTRATION_MSG, response.getSize());
  }

  /**
   * Returns the operation for the translated integer
   * @param type input type
   * @return Operation
   */
  minifi::c2::Operation getOperation(int type) const;

  /**
   * Writes a heartbeat to the provided BaseStream ptr.
   * @param stream BaseStream
   * @param payload payload to serialize
   * @return result 0 if success failure otherwise
   */
  int writeHeartbeat(io::BaseStream *stream, const minifi::c2::C2Payload &payload);

  /**
   * Writes a acknowledgement to the provided BaseStream ptr.
   * @param stream BaseStream
   * @param payload payload to serialize
   * @return result 0 if success failure otherwise
   */
  int writeAcknowledgement(io::BaseStream *stream, const minifi::c2::C2Payload &payload);

  minifi::c2::C2Payload serialize(const minifi::c2::C2Payload &payload);

  std::shared_ptr<coap::controllers::CoapConnectorService> coap_service_;

  std::mutex protocol_mutex_;

  bool require_registration_;

  std::string controller_service_name_;

 private:
  static uint8_t REGISTRATION_MSG[8];

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace c2 */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
