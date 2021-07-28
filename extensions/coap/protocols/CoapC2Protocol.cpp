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
#include "CoapC2Protocol.h"
#include "c2/PayloadSerializer.h"
#include "c2/PayloadParser.h"
#include "coap_functions.h"
#include "coap_message.h"
#include "io/BaseStream.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace coap {
namespace c2 {

uint8_t CoapProtocol::REGISTRATION_MSG[8] = { 0x72, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72 };

CoapProtocol::CoapProtocol(const std::string &name, const utils::Identifier &uuid)
    : RESTSender(name, uuid),
      require_registration_(false),
      logger_(logging::LoggerFactory<CoapProtocol>::getLogger()) {
}

CoapProtocol::~CoapProtocol() = default;

void CoapProtocol::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) {
  RESTSender::initialize(controller, configure);
  if (configure->get("nifi.c2.coap.connector.service", controller_service_name_)) {
    auto service = controller->getControllerService(controller_service_name_);
    coap_service_ = std::static_pointer_cast<coap::controllers::CoapConnectorService>(service);
  } else {
    logger_->log_info("No CoAP connector configured, so using default service");
    coap_service_ = std::make_shared<coap::controllers::CoapConnectorService>("cs", configure);
    coap_service_->onEnable();
  }
}

minifi::c2::C2Payload CoapProtocol::consumePayload(const std::string &url, const minifi::c2::C2Payload &payload, minifi::c2::Direction direction, bool /*async*/) {
  return RESTSender::consumePayload(url, payload, direction, false);
}

int CoapProtocol::writeAcknowledgement(io::BaseStream *stream, const minifi::c2::C2Payload &payload) {
  auto ident = payload.getIdentifier();
  auto state = payload.getStatus().getState();
  stream->write(ident);
  uint8_t payloadState = 0;
  switch (state) {
    case state::UpdateState::NESTED:
    case state::UpdateState::INITIATE:
    case state::UpdateState::FULLY_APPLIED:
    case state::UpdateState::READ_COMPLETE:
      payloadState = 0;
      break;
    case state::UpdateState::NOT_APPLIED:
    case state::UpdateState::PARTIALLY_APPLIED:
      payloadState = 1;
      break;
    case state::UpdateState::READ_ERROR:
      payloadState = 2;
      break;
    case state::UpdateState::SET_ERROR:
      payloadState = 3;
      break;
  }
  stream->write(&payloadState, 1);
  return 0;
}

int CoapProtocol::writeHeartbeat(io::BaseStream *stream, const minifi::c2::C2Payload &payload) {
  bool byte;
  uint16_t size = 0;

  logger_->log_trace("Writing heartbeat");

  try {
    const std::string deviceIdent = minifi::c2::PayloadParser::getInstance(payload).in("deviceInfo").getAs<std::string>("identifier");

    const std::string agentIdent = minifi::c2::PayloadParser::getInstance(payload).in("agentInfo").getAs<std::string>("identifier");

    stream->write(deviceIdent, false);

    logger_->log_trace("Writing heartbeat with device Ident %s and agent Ident %s", deviceIdent, agentIdent);

    if (agentIdent.empty()) {
      return -1;
    }
    stream->write(agentIdent, false);

    try {
      auto flowInfoParser = minifi::c2::PayloadParser::getInstance(payload).in("flowInfo");
      auto componentParser = flowInfoParser.in("components");
      auto queueParser = flowInfoParser.in("queues");
      auto vfsParser = flowInfoParser.in("versionedFlowSnapshotURI");
      byte = true;
      stream->write(byte);
      size = componentParser.getSize();
      stream->write(size);

      componentParser.foreach([this, stream](const minifi::c2::C2Payload &component) {
        auto myParser = minifi::c2::PayloadParser::getInstance(component);
        bool running = false;
        stream->write(component.getLabel());
        try {
          running = myParser.getAs<bool>("running");
        }
        catch(const minifi::c2::PayloadParseException &e) {
          logger_->log_error("Could not find running in components");
        }
        stream->write(running);
      });
      size = queueParser.getSize();
      stream->write(size);
      queueParser.foreach([this, stream](const minifi::c2::C2Payload &component) {
        auto myParser = minifi::c2::PayloadParser::getInstance(component);
        stream->write(component.getLabel());
        uint64_t datasize = 0, datasizemax = 0, qsize = 0, sizemax = 0;
        try {
          datasize = myParser.getAs<uint64_t>("dataSize");
          datasizemax = myParser.getAs<uint64_t>("dataSizeMax");
          qsize = myParser.getAs<uint64_t>("size");
          sizemax = myParser.getAs<uint64_t>("sizeMax");
        }
        catch(const minifi::c2::PayloadParseException &e) {
          logger_->log_error("Could not find queue sizes");
        }
        stream->write(datasize);
        stream->write(datasizemax);
        stream->write(qsize);
        stream->write(sizemax);
      });

      auto bucketId = vfsParser.getAs<std::string>("bucketId");
      auto flowId = vfsParser.getAs<std::string>("flowId");

      stream->write(bucketId);
      stream->write(flowId);
    } catch (const minifi::c2::PayloadParseException &pe) {
      logger_->log_error("Parser exception occurred, but is ignorable, reason %s", pe.what());
      // okay to ignore
      byte = false;
      stream->write(byte);
    }
  } catch (const minifi::c2::PayloadParseException &e) {
    logger_->log_error("Parser exception occurred, reason %s", e.what());
    return -1;
  }
  return 0;
}

minifi::c2::Operation CoapProtocol::getOperation(int type) const {
  switch (type) {
    case 0:
      return minifi::c2::Operation::ACKNOWLEDGE;
    case 1:
      return minifi::c2::Operation::HEARTBEAT;
    case 2:
      return minifi::c2::Operation::CLEAR;
    case 3:
      return minifi::c2::Operation::DESCRIBE;
    case 4:
      return minifi::c2::Operation::RESTART;
    case 5:
      return minifi::c2::Operation::START;
    case 6:
      return minifi::c2::Operation::UPDATE;
    case 7:
      return minifi::c2::Operation::STOP;
    case 8:
      return minifi::c2::Operation::PAUSE;
    case 9:
      return minifi::c2::Operation::RESUME;
  }
  return minifi::c2::Operation::ACKNOWLEDGE;
}

minifi::c2::C2Payload CoapProtocol::serialize(const minifi::c2::C2Payload &payload) {
  if (nullptr == coap_service_) {
    // return an error if we don't have a coap service
    logger_->log_error("CoAP service requested without any configured hostname or port");
    return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
  }

  if (require_registration_) {
    logger_->log_debug("Server requested agent registration, so attempting");
    auto response = minifi::c2::RESTSender::consumePayload(rest_uri_, payload, minifi::c2::TRANSMIT, false);
    if (response.getStatus().getState() == state::UpdateState::READ_ERROR) {
      logger_->log_trace("Could not register");
      return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE);
    } else {
      logger_->log_trace("Registered agent.");
    }
    require_registration_ = false;

    return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE);
  }

  uint16_t version = 0;
  uint8_t payload_type = 0;
  uint16_t size = 0;
  io::BufferStream stream;

  stream.write(version);
  std::string endpoint = "heartbeat";
  switch (payload.getOperation().value()) {
    case minifi::c2::Operation::ACKNOWLEDGE:
      endpoint = "acknowledge";
      payload_type = 0;
      stream.write(&payload_type, 1);
      if (writeAcknowledgement(&stream, payload) != 0) {
        return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
      }
      break;
    case minifi::c2::Operation::HEARTBEAT:
      payload_type = 1;
      stream.write(&payload_type, 1);
      if (writeHeartbeat(&stream, payload) != 0) {
        logger_->log_error("Could not write heartbeat");
        return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
      }
      break;
    default:
      logger_->log_error("Could not identify operation");
      return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
  }

  size_t bsize = stream.size();

  CoapMessage msg;
  msg.data_ = const_cast<uint8_t *>(stream.getBuffer());
  msg.size_ = bsize;

  coap::controllers::CoapResponse message = coap_service_->sendPayload(COAP_REQUEST_POST, endpoint, &msg);

  if (isRegistrationMessage(message)) {
    require_registration_ = true;
  } else if (message.getSize() > 0) {
    io::BufferStream responseStream(message.getData(), message.getSize());
    responseStream.read(version);
    responseStream.read(size);
    logger_->log_trace("Received ack. version %d. number of operations %d", version, size);
    minifi::c2::C2Payload new_payload(payload.getOperation(), state::UpdateState::NESTED);
    for (int i = 0; i < size; i++) {
      uint8_t operationType;
      uint16_t argsize = 0;
      std::string operand, id;
      REQUIRE_SIZE_IF(1, responseStream.read(operationType))
      REQUIRE_VALID(responseStream.read(id, false))
      REQUIRE_VALID(responseStream.read(operand, false))

      logger_->log_trace("Received op %d, with id %s and operand %s", operationType, id, operand);
      auto newOp = getOperation(operationType);
      minifi::c2::C2Payload nested_payload(newOp, state::UpdateState::READ_COMPLETE);
      nested_payload.setIdentifier(id);
      minifi::c2::C2ContentResponse new_command(newOp);
      new_command.delay = 0;
      new_command.required = true;
      new_command.ttl = -1;
      new_command.name = operand;
      new_command.ident = id;
      responseStream.read(argsize);
      for (int j = 0; j < argsize; j++) {
        std::string key, value;
        REQUIRE_VALID(responseStream.read(key))
        REQUIRE_VALID(responseStream.read(value))
        new_command.operation_arguments[key] = value;
      }

      nested_payload.addContent(std::move(new_command));
      new_payload.addPayload(std::move(nested_payload));
    }
    return new_payload;
  }

  return minifi::c2::C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
}

REGISTER_INTERNAL_RESOURCE(CoapProtocol);

} /* namespace c2 */
} /* namespace coap */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
