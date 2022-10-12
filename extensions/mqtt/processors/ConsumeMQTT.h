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

#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "concurrentqueue.h"
#include "AbstractMQTTProcessor.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

#define MQTT_TOPIC_ATTRIBUTE "mqtt.topic"
#define MQTT_BROKER_ATTRIBUTE "mqtt.broker"

class ConsumeMQTT : public processors::AbstractMQTTProcessor {
 public:
  explicit ConsumeMQTT(std::string name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(std::move(name), uuid) {
    maxQueueSize_ = 100;
  }

  EXTENSIONAPI static constexpr const char* Description = "This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. "
      "The the payload of the MQTT message becomes content of a FlowFile";

  EXTENSIONAPI static const core::Property CleanSession;
  EXTENSIONAPI static const core::Property QueueBufferMaxMessage;

  static auto properties() {
    return utils::array_cat(AbstractMQTTProcessor::properties(), std::array{
      CleanSession,
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

 private:
  struct MQTTMessageDeleter {
    void operator()(MQTTAsync_message* message) {
      MQTTAsync_freeMessage(&message);
    }
  };

  void getReceivedMQTTMsg(std::deque<std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter>>& msg_queue) {
    std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter> message;
    while (queue_.try_dequeue(message)) {
      msg_queue.push_back(std::move(message));
    }
  }

  // MQTT async callback
  static void subscriptionSuccess(void* context, MQTTAsync_successData* response) {
    auto* processor = reinterpret_cast<ConsumeMQTT*>(context);
    processor->onSubscriptionSuccess(response);
  }

  // MQTT async callback
  static void subscriptionFailure(void* context, MQTTAsync_failureData* response) {
    auto* processor = reinterpret_cast<ConsumeMQTT*>(context);
    processor->onSubscriptionFailure(response);
  }

  void onSubscriptionSuccess(MQTTAsync_successData* /*response*/) {
    logger_->log_info("Successfully subscribed to MQTT topic %s on broker %s", topic_, uri_);
  }

  void onSubscriptionFailure(MQTTAsync_failureData* response) {
    logger_->log_error("Subscription failed on topic %s to MQTT broker %s (%d)", topic_, uri_, response->code);
    if (response->message != nullptr) {
      logger_->log_error("Detailed reason for subscription failure: %s", response->message);
    }
  }

  bool getCleanSession() const override {
    return cleanSession_;
  }

  void onMessageReceived(char* topic_name, int /*topic_len*/, MQTTAsync_message* message) override;

  void enqueueReceivedMQTTMsg(std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter> message);

  bool startupClient() override;

  void checkProperties() override;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeMQTT>::getLogger();
  bool cleanSession_ = true;
  uint64_t maxQueueSize_;
  moodycamel::ConcurrentQueue<std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter>> queue_;
};

}  // namespace org::apache::nifi::minifi::processors
