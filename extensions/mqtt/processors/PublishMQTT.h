/**
 * @file PublishMQTT.h
 * PublishMQTT class declaration
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
#pragma once

#include <vector>
#include <string>
#include <memory>

#include <limits>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "MQTTClient.h"
#include "AbstractMQTTProcessor.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// PublishMQTT Class
class PublishMQTT : public processors::AbstractMQTTProcessor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit PublishMQTT(const std::string& name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(name, uuid),
        logger_(logging::LoggerFactory<PublishMQTT>::getLogger()) {
    retain_ = false;
    max_seg_size_ = ULLONG_MAX;
  }
  // Destructor
  ~PublishMQTT() override = default;
  // Processor Name
  static constexpr char const* ProcessorName = "PublishMQTT";
  // Supported Properties
  static core::Property Retain;
  static core::Property MaxFlowSegSize;

  static core::Relationship Failure;
  static core::Relationship Success;

  // Nest Callback Class for read stream
  class ReadCallback : public InputStreamCallback {
   public:
    ReadCallback(uint64_t flow_size, uint64_t max_seg_size, const std::string &key, MQTTClient client, int qos, bool retain, MQTTClient_deliveryToken &token)
        : flow_size_(flow_size),
          max_seg_size_(max_seg_size),
          key_(key),
          client_(client),
          qos_(qos),
          retain_(retain),
          token_(token) {
      status_ = 0;
      read_size_ = 0;
    }
    ~ReadCallback() override = default;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (flow_size_ < max_seg_size_)
        max_seg_size_ = flow_size_;
      gsl_Expects(max_seg_size_ < gsl::narrow<uint64_t>(std::numeric_limits<int>::max()));
      std::vector<unsigned char> buffer(max_seg_size_);
      read_size_ = 0;
      status_ = 0;
      while (read_size_ < flow_size_) {
        // MQTTClient_message::payloadlen is int, so we can't handle 2GB+
        const auto readRet = stream->read(&buffer[0], max_seg_size_);
        if (io::isError(readRet)) {
          status_ = -1;
          return gsl::narrow<int64_t>(read_size_);
        }
        if (readRet > 0) {
          MQTTClient_message pubmsg = MQTTClient_message_initializer;
          pubmsg.payload = &buffer[0];
          pubmsg.payloadlen = gsl::narrow<int>(readRet);
          pubmsg.qos = qos_;
          pubmsg.retained = retain_;
          if (MQTTClient_publishMessage(client_, key_.c_str(), &pubmsg, &token_) != MQTTCLIENT_SUCCESS) {
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
    std::string key_;
    MQTTClient client_;

    int status_;
    size_t read_size_;
    int qos_;
    int retain_;
    MQTTClient_deliveryToken &token_;
  };

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  // OnTrigger method, implemented by NiFi PublishMQTT
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi PublishMQTT
  void initialize(void) override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_REQUIRED;
  }

  uint64_t max_seg_size_;
  bool retain_;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
