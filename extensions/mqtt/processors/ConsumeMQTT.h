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

#include <limits>
#include <string>
#include <memory>

#include <deque>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "concurrentqueue.h"
#include "MQTTClient.h"
#include "AbstractMQTTProcessor.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

#define MQTT_TOPIC_ATTRIBUTE "mqtt.topic"
#define MQTT_BROKER_ATTRIBUTE "mqtt.broker"

class ConsumeMQTT : public processors::AbstractMQTTProcessor {
 public:
  explicit ConsumeMQTT(const std::string& name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(name, uuid) {
    maxQueueSize_ = 100;
    maxSegSize_ = ULLONG_MAX;
  }

  ~ConsumeMQTT() override {
    MQTTClient_message *message;
    while (queue_.try_dequeue(message)) {
      MQTTClient_freeMessage(&message);
    }
  }
  static core::Property CleanSession;
  static core::Property MaxFlowSegSize;
  static core::Property QueueBufferMaxMessage;

  EXTENSIONAPI static constexpr const char* Description = "This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. "
      "The the payload of the MQTT message becomes content of a FlowFile";

  EXTENSIONAPI static const core::Property MaxFlowSegSize;
  EXTENSIONAPI static const core::Property QueueBufferMaxMessage;
  static auto properties() {
    return utils::array_cat(AbstractMQTTProcessor::properties(), std::array{
      MaxFlowSegSize,
      QueueBufferMaxMessage
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  bool enqueueReceiveMQTTMsg(MQTTClient_message *message) override;

 protected:
  void getReceivedMQTTMsg(std::deque<MQTTClient_message *> &msg_queue) {
    MQTTClient_message *message;
    while (queue_.try_dequeue(message)) {
      msg_queue.push_back(message);
    }
  }

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  bool getCleanSession() const override {
    return cleanSession_;
  }

  void onMessageReceived(MQTTClient_message* message) override {
    // TODO(amarkovics) MQTT messages should be stored in a unique_ptr with custom deleter
    if (!enqueueReceiveMQTTMsg(message)) {
      MQTTClient_freeMessage(&message);
    }
  }

  bool startupClient() override;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeMQTT>::getLogger();
  std::mutex mutex_;
  bool cleanSession_ = true;
  uint64_t maxQueueSize_;
  uint64_t maxSegSize_;
  moodycamel::ConcurrentQueue<MQTTClient_message *> queue_;
};

}  // namespace org::apache::nifi::minifi::processors
