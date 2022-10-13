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
      : processors::AbstractMQTTProcessor(std::move(name), uuid) {
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
  class ReadCallback {
   public:
    ReadCallback(PublishMQTT* processor, std::shared_ptr<core::FlowFile> flow_file, std::string topic, std::string content_type)
        : processor_(processor),
          flow_file_(std::move(flow_file)),
          topic_(std::move(topic)),
          content_type_(std::move(content_type)) {
    }

    int64_t operator()(const std::shared_ptr<io::InputStream>& stream);

    bool sendMessage(const MQTTAsync_message* message_to_publish, MQTTAsync_responseOptions* response_options);

    [[nodiscard]] size_t getReadSize() const {
      return read_size_;
    }

    [[nodiscard]] bool getSuccessStatus() const {
      return success_status_;
    }

   private:
    // MQTT static async callbacks, calling their notify with context being pointer to a ReadCallback::Context object
    static void sendSuccess(void* context, MQTTAsync_successData* response);
    static void sendSuccess5(void* context, MQTTAsync_successData5* response);
    static void sendFailure(void* context, MQTTAsync_failureData* response);
    static void sendFailure5(void* context, MQTTAsync_failureData5* response);

    void notify(bool success, std::optional<int> response_code, std::optional<MQTTReasonCodes> reason_code);
    void setMqtt5Properties(MQTTAsync_message& message) const;
    void addAttributesAsUserProperties(MQTTAsync_message& message) const;

    PublishMQTT* processor_;
    uint64_t read_size_ = 0;
    std::shared_ptr<core::FlowFile> flow_file_;
    std::string topic_;
    std::string content_type_;

    bool success_status_ = true;
  };

  class InFlightMessageCounter {
   public:
    void setMqttVersion(const MqttVersions mqtt_version) {
      mqtt_version_ = mqtt_version;
    }

    void setQoS(const MqttQoS qos) {
      qos_ = qos;
    }

    void setMax(const uint16_t new_limit) {
      limit_ = new_limit;
    }

    // increase on sending, wait if limit is reached
    void increase();

    // decrease on success or failure, notify
    void decrease();

   private:
    std::mutex mutex_;
    std::condition_variable cv_;
    uint16_t counter_{0};
    uint16_t limit_{65535};
    MqttVersions mqtt_version_;
    MqttQoS qos_;
  };

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

  /**
   * Resolves topic from expression language
   */
  std::string getTopic(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  /**
   * Resolves content type from expression language
   */
  std::string getContentType(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const;

  bool retain_ = false;
  std::optional<std::chrono::seconds> message_expiry_interval_;
  InFlightMessageCounter in_flight_message_counter_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PublishMQTT>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
