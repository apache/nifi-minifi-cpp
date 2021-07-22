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
#include "MQTTC2Protocol.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

MQTTC2Protocol::MQTTC2Protocol(const std::string& name, const utils::Identifier& uuid)
    : C2Protocol(name, uuid),
      logger_(logging::LoggerFactory<MQTTC2Protocol>::getLogger()) {
}

MQTTC2Protocol::~MQTTC2Protocol() = default;

void MQTTC2Protocol::initialize(core::controller::ControllerServiceProvider* controller, const std::shared_ptr<Configure> &configure) {
  if (configure->get("nifi.c2.mqtt.connector.service", controller_service_name_)) {
    auto service = controller->getControllerService(controller_service_name_);
    mqtt_service_ = std::static_pointer_cast<controllers::MQTTControllerService>(service);
  } else {
    mqtt_service_ = nullptr;
  }

  agent_identifier_ = configure->getAgentIdentifier();

  std::stringstream outputStream;
  std::string updateTopicOpt, heartbeatTopicOpt;
  if (configure->get("nifi.c2.mqtt.heartbeat.topic", heartbeatTopicOpt)) {
    heartbeat_topic_ = heartbeatTopicOpt;
  } else {
    heartbeat_topic_ = "heartbeats";  // outputStream.str();
  }
  if (configure->get("nifi.c2.mqtt.update.topic", updateTopicOpt)) {
    update_topic_ = updateTopicOpt;
  } else {
    update_topic_ = "updates";
  }

  std::stringstream inputStream;
  inputStream << agent_identifier_ << "/in";
  in_topic_ = inputStream.str();

  if (mqtt_service_) {
    mqtt_service_->subscribeToTopic(in_topic_);
  }
}

C2Payload MQTTC2Protocol::consumePayload(const std::string &url, const C2Payload &payload, Direction /*direction*/, bool /*async*/) {
  // we are getting an update.
  std::lock_guard<std::mutex> lock(input_mutex_);
  io::BufferStream stream;
  stream.write(in_topic_);
  stream.write(url);
  std::vector<uint8_t> response;
  auto transmit_id = mqtt_service_->send(update_topic_, stream.getBuffer(), stream.size());
  if (transmit_id > 0 && mqtt_service_->awaitResponse(5000, transmit_id, in_topic_, response)) {
    C2Payload response_payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true);
    response_payload.setRawData(response);
    return response_payload;
  } else {
    return C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE);
  }
}

C2Payload MQTTC2Protocol::serialize(const C2Payload &payload) {
  if (mqtt_service_ == nullptr || !mqtt_service_->isRunning()) {
    return C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
  }

  std::lock_guard<std::mutex> lock(input_mutex_);

  auto stream = c2::PayloadSerializer::serialize(0x00, payload);

  auto transmit_id = mqtt_service_->send(heartbeat_topic_, stream->getBuffer(), stream->size());
  std::vector<uint8_t> response;
  if (transmit_id > 0 && mqtt_service_->awaitResponse(5000, transmit_id, in_topic_, response)) {
    return c2::PayloadSerializer::deserialize(response);
  }
  return C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR);
}

REGISTER_INTERNAL_RESOURCE(MQTTC2Protocol);

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
