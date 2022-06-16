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
  explicit PublishMQTT(const std::string& name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(name, uuid) {
    retain_ = false;
    max_seg_size_ = ULLONG_MAX;
  }
  ~PublishMQTT() override = default;

  EXTENSIONAPI static constexpr const char* Description = "PublishMQTT serializes FlowFile content as an MQTT payload, sending the message to the configured topic and broker.";

  EXTENSIONAPI static const core::Property Retain;
  EXTENSIONAPI static const core::Property MaxFlowSegSize;

  static auto properties() {
    return utils::array_cat(AbstractMQTTProcessor::properties(), std::array{
      Retain,
      MaxFlowSegSize
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
    ReadCallback(uint64_t flow_size, uint64_t max_seg_size, std::string topic, MQTTAsync client, int qos, bool retain, MQTTAsync_token &token)
        : flow_size_(flow_size),
          max_seg_size_(max_seg_size),
          topic_(std::move(topic)),
          client_(client),
          qos_(qos),
          retain_(retain),
          token_(token) {
      status_ = 0;
      read_size_ = 0;
    }
    int64_t operator()(const std::shared_ptr<io::BaseStream>& stream) {
      if (flow_size_ < max_seg_size_)
        max_seg_size_ = flow_size_;
      gsl_Expects(max_seg_size_ < gsl::narrow<uint64_t>(std::numeric_limits<int>::max()));
      std::vector<std::byte> buffer(max_seg_size_);
      read_size_ = 0;
      status_ = 0;
      while (read_size_ < flow_size_) {
        // MQTTClient_message::payloadlen is int, so we can't handle 2GB+
        const auto readRet = stream->read(buffer);
        if (io::isError(readRet)) {
          status_ = -1;
          return gsl::narrow<int64_t>(read_size_);
        }
        if (readRet > 0) {
          MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
          pubmsg.payload = buffer.data();
          pubmsg.payloadlen = gsl::narrow<int>(readRet);
          pubmsg.qos = qos_;
          pubmsg.retained = retain_;
            MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
            response_options.token = token_;
            // TODO(amarkovics) set callbacks
          if (MQTTAsync_sendMessage(client_, topic_.c_str(), &pubmsg, &response_options) != MQTTASYNC_SUCCESS) {
            status_ = -1;
            return -1;
          }
          read_size_ += gsl::narrow<size_t>(readRet);
        } else {
          break;
        }
      }
      return gsl::narrow<int64_t>(read_size_);
    }
    uint64_t flow_size_;
    uint64_t max_seg_size_;
    std::string topic_;
    MQTTAsync client_;

    int status_;
    size_t read_size_;
    int qos_;
    int retain_;
    MQTTAsync_token& token_;
  };

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  bool getCleanSession() const override {
    return true;
  }

  void onMessageReceived(MQTTAsync_message* message) override {
    MQTTAsync_freeMessage(&message);
  }

  bool startupClient() override {
    // there is no need to do anything like subscribe on the beginning
    return true;
  }

  uint64_t max_seg_size_;
  bool retain_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PublishMQTT>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
