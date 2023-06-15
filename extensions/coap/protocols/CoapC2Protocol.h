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
#include "io/OutputStream.h"
#include "agent/agent_version.h"
#include "CoapConnector.h"

#include "coap2/coap.h"
#include "coap2/uri.h"
#include "coap2/address.h"
#include "protocols/RESTSender.h"

namespace org::apache::nifi::minifi::coap::c2 {

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
  explicit CoapProtocol(std::string name, const utils::Identifier &uuid = utils::Identifier());

  ~CoapProtocol() override;

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;

  minifi::c2::C2Payload consumePayload(const std::string &url, const minifi::c2::C2Payload &payload, minifi::c2::Direction direction, bool async) override;

  minifi::c2::C2Payload consumePayload(const minifi::c2::C2Payload &payload, minifi::c2::Direction /*direction*/, bool /*async*/) override {
      return serialize(payload);
  }

  void update(const std::shared_ptr<Configure>& /*configure*/) override {
    // no op.
  }

  void initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) override;

 protected:
  static bool isRegistrationMessage(controllers::CoapResponse &response) {
    const auto response_data = response.getData();
    if (LIKELY(response_data.size() != 8)) {
      return false;
    }
    return response.getCode() == COAP_RESPONSE_400 && !memcmp(response_data.data(), REGISTRATION_MSG, response_data.size());
  }

  /**
   * Returns the operation for the translated integer
   * @param type input type
   * @return Operation
   */
  static minifi::c2::Operation getOperation(int type);

  /**
   * Writes a heartbeat to the provided OutputStream ptr.
   * @param stream OutputStream
   * @param payload payload to serialize
   * @return result 0 if success failure otherwise
   */
  int writeHeartbeat(io::OutputStream *stream, const minifi::c2::C2Payload &payload);

  /**
   * Writes a acknowledgement to the provided OutputStream ptr.
   * @param stream OutputStream
   * @param payload payload to serialize
   * @return result 0 if success failure otherwise
   */
  static int writeAcknowledgement(io::OutputStream *stream, const minifi::c2::C2Payload &payload);

  minifi::c2::C2Payload serialize(const minifi::c2::C2Payload &payload);

  std::shared_ptr<coap::controllers::CoapConnectorService> coap_service_;

  std::mutex protocol_mutex_;

  bool require_registration_;

  std::string controller_service_name_;

 private:
  static uint8_t REGISTRATION_MSG[8];

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<CoapProtocol>::getLogger();
};

}  // namespace org::apache::nifi::minifi::coap::c2
