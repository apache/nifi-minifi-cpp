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

#include <vector>
#include <string>
#include <memory>
#include <utility>

#include <limits>

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

  EXTENSIONAPI static const core::Property Retain;

  static auto properties() {
    return utils::array_cat(AbstractMQTTProcessor::properties(), std::array{
      Retain
    });
  }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  class ReadCallback {
   public:
    ReadCallback(PublishMQTT* processor, uint64_t flow_size, uint64_t max_seg_size, std::string topic, MQTTAsync client, int qos, bool retain)
        : processor_(processor),
          flow_size_(flow_size),
          max_seg_size_(max_seg_size),
          topic_(std::move(topic)),
          client_(client),
          qos_(qos),
          retain_(retain) {
    }

    int64_t operator()(const std::shared_ptr<io::InputStream>& stream);

    size_t read_size_ = 0;
    int status_ = 0;

   private:
    PublishMQTT* processor_;
    uint64_t flow_size_;
    uint64_t max_seg_size_;
    std::string topic_;
    MQTTAsync client_;

    int qos_;
    bool retain_;
  };

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  // MQTT async callback
  static void sendSuccess(void* context, MQTTAsync_successData* response) {
    auto* processor = reinterpret_cast<PublishMQTT*>(context);
    processor->onSendSuccess(response);
  }

  // MQTT async callback
  static void sendFailure(void* context, MQTTAsync_failureData* response) {
    auto* processor = reinterpret_cast<PublishMQTT*>(context);
    processor->onSendFailure(response);
  }

  void onSendSuccess(MQTTAsync_successData* /*response*/) {
    logger_->log_debug("Successfully sent message to MQTT topic %s on broker %s", topic_, uri_);
  }

  void onSendFailure(MQTTAsync_failureData* response) {
    logger_->log_error("Sending message failed on topic %s to MQTT broker %s (%d)", topic_, uri_, response->code);
    if (response->message != nullptr) {
      logger_->log_error("Detailed reason for sending failure: %s", response->message);
    }
  }

  bool getCleanSession() const override {
    return true;
  }

  bool startupClient() override {
    // there is no need to do anything like subscribe on the beginning
    return true;
  }

  bool retain_ = false;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PublishMQTT>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
