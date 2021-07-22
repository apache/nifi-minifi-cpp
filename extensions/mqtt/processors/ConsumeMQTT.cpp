/**
 * @file ConsumeMQTT.cpp
 * ConsumeMQTT class implementation
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
#include "ConsumeMQTT.h"

#include <stdio.h>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
#include <set>
#include <cinttypes>

#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ConsumeMQTT::MaxFlowSegSize("Max Flow Segment Size", "Maximum flow content payload segment size for the MQTT record", "");
core::Property ConsumeMQTT::QueueBufferMaxMessage("Queue Max Message", "Maximum number of messages allowed on the received MQTT queue", "");

core::Relationship ConsumeMQTT::Success("success", "FlowFiles that are sent successfully to the destination are transferred to this relationship");

void ConsumeMQTT::initialize() {
  // Set the supported properties
  std::set<core::Property> properties(AbstractMQTTProcessor::getSupportedProperties());
  properties.insert(MaxFlowSegSize);
  properties.insert(QueueBufferMaxMessage);
  setSupportedProperties(properties);
  // Set the supported relationships
  setSupportedRelationships({Success});
}

bool ConsumeMQTT::enqueueReceiveMQTTMsg(MQTTClient_message *message) {
  if (queue_.size_approx() >= maxQueueSize_) {
    logger_->log_warn("MQTT queue full");
    return false;
  } else {
    if (gsl::narrow<uint64_t>(message->payloadlen) > maxSegSize_)
      message->payloadlen = maxSegSize_;
    queue_.enqueue(message);
    logger_->log_debug("enqueue MQTT message length %d", message->payloadlen);
    return true;
  }
}

void ConsumeMQTT::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
  AbstractMQTTProcessor::onSchedule(context, factory);
  std::string value;
  int64_t valInt;
  value = "";
  if (context->getProperty(QueueBufferMaxMessage.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    maxQueueSize_ = valInt;
    logger_->log_debug("ConsumeMQTT: Queue Max Message [%" PRIu64 "]", maxQueueSize_);
  }
  value = "";
  if (context->getProperty(MaxFlowSegSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    maxSegSize_ = valInt;
    logger_->log_debug("ConsumeMQTT: Max Flow Segment Size [%" PRIu64 "]", maxSegSize_);
  }
}

void ConsumeMQTT::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession> &session) {
  // reconnect if necessary
  if (!reconnect()) {
    yield();
  }

  std::deque<MQTTClient_message *> msg_queue;
  getReceivedMQTTMsg(msg_queue);
  while (!msg_queue.empty()) {
    MQTTClient_message *message = msg_queue.front();
    std::shared_ptr<core::FlowFile> processFlowFile = session->create();
    ConsumeMQTT::WriteCallback callback(message);
    session->write(processFlowFile, &callback);
    if (callback.status_ < 0) {
      logger_->log_error("ConsumeMQTT fail for the flow with UUID %s", processFlowFile->getUUIDStr());
      session->remove(processFlowFile);
    } else {
      session->putAttribute(processFlowFile, MQTT_BROKER_ATTRIBUTE, uri_.c_str());
      session->putAttribute(processFlowFile, MQTT_TOPIC_ATTRIBUTE, topic_.c_str());
      logger_->log_debug("ConsumeMQTT processing success for the flow with UUID %s topic %s", processFlowFile->getUUIDStr(), topic_);
      session->transfer(processFlowFile, Success);
    }
    MQTTClient_freeMessage(&message);
    msg_queue.pop_front();
  }
}

REGISTER_RESOURCE(ConsumeMQTT, "This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. The the payload of the MQTT message becomes content of a FlowFile");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
