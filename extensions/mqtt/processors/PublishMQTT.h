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
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "AbstractMQTTProcessor.h"
#include "utils/ArrayUtils.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::processors {

class PublishMQTT : public processors::AbstractMQTTProcessor {
 public:
  explicit PublishMQTT(std::string name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(std::move(name), uuid, std::make_shared<PublishMQTTMetrics>(*this, in_flight_message_counter_)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "PublishMQTT serializes FlowFile content as an MQTT payload, sending the message to the configured topic and broker.";

  EXTENSIONAPI static const core::Property Topic;
  EXTENSIONAPI static const core::Property Retain;
  EXTENSIONAPI static const core::Property MessageExpiryInterval;
  EXTENSIONAPI static const core::Property ContentType;

  static auto properties() {
    return utils::array_cat(AbstractMQTTProcessor::basicProperties(), std::array{
      Topic,
      Retain,
      MessageExpiryInterval,
      ContentType
    }, AbstractMQTTProcessor::advancedProperties());
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void readProperties(const std::shared_ptr<core::ProcessContext>& context) override;
  void onTriggerImpl(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
  void initialize() override;

 private:
  /**
   * Counts unacknowledged QoS 1 and QoS 2 messages to respect broker's Receive Maximum
   */
  class InFlightMessageCounter {
   public:
    void setEnabled(bool status) { enabled_ = status; }

    void setMax(uint16_t new_limit);
    void increase();
    void decrease();

    uint16_t getCounter() const;

   private:
    bool enabled_ = false;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    uint16_t counter_{0};
    uint16_t limit_{MQTT_MAX_RECEIVE_MAXIMUM};
  };

  class PublishMQTTMetrics : public core::ProcessorMetrics {
   public:
    PublishMQTTMetrics(const core::Processor& source_processor, const InFlightMessageCounter& in_flight_message_counter);
    std::vector<state::response::SerializedResponseNode> serialize() override;
    std::vector<state::PublishedMetric> calculateMetrics() override;

   private:
    gsl::not_null<const InFlightMessageCounter*> in_flight_message_counter_;
  };

  // MQTT static async callbacks, calling their notify with context being pointer to a packaged_task to notify()
  static void sendSuccess(void* context, MQTTAsync_successData* response);
  static void sendSuccess5(void* context, MQTTAsync_successData5* response);
  static void sendFailure(void* context, MQTTAsync_failureData* response);
  static void sendFailure5(void* context, MQTTAsync_failureData5* response);

  /**
   * Resolves topic from expression language
   */
  std::string getTopic(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  /**
   * Resolves content type from expression language
   */
  std::string getContentType(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  /**
   * Sends an MQTT message asynchronously
   * @param buffer contents of the message
   * @param topic topic of the message
   * @param content_type Content Type for MQTT 5
   * @param flow_file Flow File being processed
   * @return success of message sending
   */
  bool sendMessage(const std::vector<std::byte>& buffer, const std::string& topic, const std::string& content_type, const std::shared_ptr<core::FlowFile>& flow_file);

  /**
   * Callback for asynchronous message sending
   * @param success if message sending was successful
   * @param response_code response code for failure only
   * @param reason_code MQTT 5 reason code
   * @return if message sending was successful
   */
  bool notify(bool success, std::optional<int> response_code, std::optional<MQTTReasonCodes> reason_code);

  /**
   * Set MQTT 5-exclusive properties
   * @param message message object
   * @param content_type content type
   * @param flow_file Flow File being processed
   */
  void setMqtt5Properties(MQTTAsync_message& message, const std::string& content_type, const std::shared_ptr<core::FlowFile>& flow_file) const;

  /**
   * Adds flow file attributes as user properties to an MQTT 5 message
   * @param message message object
   * @param flow_file Flow File being processed
   */
  static void addAttributesAsUserProperties(MQTTAsync_message& message, const std::shared_ptr<core::FlowFile>& flow_file);

  bool getCleanSession() const override {
    return true;
  }

  bool getCleanStart() const override {
    return true;
  }

  std::chrono::seconds getSessionExpiryInterval() const override {
    // non-persistent session as we only publish
    return std::chrono::seconds{0};
  }

  void startupClient() override {
    // there is no need to do anything like subscribe in the beginning
  }

  void checkProperties() override;
  void checkBrokerLimitsImpl() override;

  bool retain_ = false;
  std::optional<std::chrono::seconds> message_expiry_interval_;
  InFlightMessageCounter in_flight_message_counter_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PublishMQTT>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
