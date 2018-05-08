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
#ifndef EXTENSIONS_MQTT_PROTOCOL_MQTTC2PROTOCOL_H_
#define EXTENSIONS_MQTT_PROTOCOL_MQTTC2PROTOCOL_H_

#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>

#include "../controllerservice/MQTTControllerService.h"
#include "c2/C2Protocol.h"
#include "io/BaseStream.h"
#include "agent/agent_version.h"
#include "PayloadSerializer.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

/**
 * Purpose: Implementation of the MQTT C2 protocol. Serializes messages to and from
 * and mqtt server.
 */
class MQTTC2Protocol : public C2Protocol {
 public:
  explicit MQTTC2Protocol(std::string name, uuid_t uuid = nullptr);

  virtual ~MQTTC2Protocol();

  /**
   * Consume the payload.
   * @param url to evaluate.
   * @param payload payload to consume.
   * @direction direction of operation.
   */
  virtual C2Payload consumePayload(const std::string &url, const C2Payload &payload, Direction direction, bool async) override;

  virtual C2Payload consumePayload(const C2Payload &payload, Direction direction, bool async) override {
    return serialize(payload);
  }

  virtual void update(const std::shared_ptr<Configure> &configure) override {
    // no op.
  }

  virtual void initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<Configure> &configure) override;

 protected:

  C2Payload serialize(const C2Payload &payload);

  std::mutex input_mutex_;
  // input topic on which we will listen.
  std::string in_topic_;
  // agent identifier
  std::string agent_identifier_;
  // heartbeat topic name.
  std::string heartbeat_topic_;
  // update topic name.
  std::string update_topic_;

  // mqtt controller service reference.
  std::shared_ptr<controllers::MQTTControllerService> mqtt_service_;
  std::shared_ptr<logging::Logger> logger_;
  //mqtt controller serviec name.
  std::string controller_service_name_;


};
} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* EXTENSIONS_MQTT_PROTOCOL_MQTTC2PROTOCOL_H_ */
