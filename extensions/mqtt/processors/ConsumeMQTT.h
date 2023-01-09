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
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
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

class ConsumeMQTT : public processors::AbstractMQTTProcessor {
 public:
  explicit ConsumeMQTT(std::string name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. "
      "The the payload of the MQTT message becomes content of a FlowFile";

  EXTENSIONAPI static const core::Property Topic;
  EXTENSIONAPI static const core::Property CleanSession;
  EXTENSIONAPI static const core::Property CleanStart;
  EXTENSIONAPI static const core::Property SessionExpiryInterval;
  EXTENSIONAPI static const core::Property QueueBufferMaxMessage;
  EXTENSIONAPI static const core::Property AttributeFromContentType;
  EXTENSIONAPI static const core::Property TopicAliasMaximum;
  EXTENSIONAPI static const core::Property ReceiveMaximum;

  static auto properties() {
    return utils::array_cat(AbstractMQTTProcessor::basicProperties(), std::array{
      Topic,
      CleanSession,
      CleanStart,
      SessionExpiryInterval,
      QueueBufferMaxMessage,
      AttributeFromContentType,
      TopicAliasMaximum,
      ReceiveMaximum
    }, AbstractMQTTProcessor::advancedProperties());
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  static constexpr const char* const MQTT_TOPIC_ATTRIBUTE = "mqtt.topic";
  static constexpr const char* const MQTT_BROKER_ATTRIBUTE = "mqtt.broker";

  void readProperties(const std::shared_ptr<core::ProcessContext>& context) override;
  void onTriggerImpl(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void initialize() override;

 private:
  class WriteCallback {
   public:
    explicit WriteCallback(const SmartMessage& message, std::shared_ptr<core::logging::Logger> logger)
      : message_(message)
      , logger_(std::move(logger)) {
    }

    int64_t operator() (const std::shared_ptr<io::OutputStream>& stream);

    [[nodiscard]] bool getSuccessStatus() const {
      return success_status_;
    }

   private:
    const SmartMessage& message_;
    std::shared_ptr<core::logging::Logger> logger_;
    bool success_status_ = true;
  };

  // MQTT static async callbacks, calling their non-static counterparts with context being pointer to "this"
  static void subscriptionSuccess(void* context, MQTTAsync_successData* response);
  static void subscriptionSuccess5(void* context, MQTTAsync_successData5* response);
  static void subscriptionFailure(void* context, MQTTAsync_failureData* response);
  static void subscriptionFailure5(void* context, MQTTAsync_failureData5* response);

  // MQTT non-static async callbacks
  void onSubscriptionSuccess();
  void onSubscriptionFailure(MQTTAsync_failureData* response);
  void onSubscriptionFailure5(MQTTAsync_failureData5* response);
  void onMessageReceived(SmartMessage smart_message) override;

  /**
   * Enqueues received MQTT message into internal message queue.
   * Called as a callback on a separate thread than onTrigger, as a reaction to message incoming.
   * @param message message to put to queue
   */
  void enqueueReceivedMQTTMsg(SmartMessage message);

  /**
   * Called in onTrigger to return the whole internal message queue
   * @return message queue of messages received since previous onTrigger
   */
  std::queue<SmartMessage> getReceivedMqttMessages();

  /**
   * Subscribes to topic
   */
  void startupClient() override;

  void checkProperties() override;
  void checkBrokerLimitsImpl() override;

  /**
   * Resolve topic name if it was sent with an alias instead of a regular topic name
   * @param smart_message message to process
   */
  void resolveTopicFromAlias(SmartMessage& smart_message);

  bool getCleanSession() const override {
    return clean_session_;
  }

  bool getCleanStart() const override {
    return clean_start_;
  }

  std::chrono::seconds getSessionExpiryInterval() const override {
    return session_expiry_interval_;
  }

  /**
   * Turn MQTT 5 User Properties to Flow File attributes
   */
  void putUserPropertiesAsAttributes(const SmartMessage& message, const std::shared_ptr<core::FlowFile>& flow_file, const std::shared_ptr<core::ProcessSession>& session) const;

  /**
   * Fill a user-requested Flow File attribute from content type
   */
  void fillAttributeFromContentType(const SmartMessage& message, const std::shared_ptr<core::FlowFile>& flow_file, const std::shared_ptr<core::ProcessSession>& session) const;

  void setProcessorSpecificMqtt5ConnectOptions(MQTTProperties& connect_props) const override;

  std::string topic_;
  bool clean_session_ = true;
  bool clean_start_ = true;
  std::chrono::seconds session_expiry_interval_{0};
  uint64_t max_queue_size_ = 1000;
  std::string attribute_from_content_type_;

  uint16_t topic_alias_maximum_{0};
  uint16_t receive_maximum_{MQTT_MAX_RECEIVE_MAXIMUM};
  std::unordered_map<uint16_t, std::string> alias_to_topic_;

  moodycamel::ConcurrentQueue<SmartMessage> queue_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConsumeMQTT>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
