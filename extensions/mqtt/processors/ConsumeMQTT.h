/**
 * @file ConsumeMQTT.h
 * ConsumeMQTT class declaration
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
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

#define MQTT_TOPIC_ATTRIBUTE "mqtt.topic"
#define MQTT_BROKER_ATTRIBUTE "mqtt.broker"

// ConsumeMQTT Class
class ConsumeMQTT : public processors::AbstractMQTTProcessor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit ConsumeMQTT(const std::string& name, const utils::Identifier& uuid = {})
      : processors::AbstractMQTTProcessor(name, uuid),
        logger_(logging::LoggerFactory<ConsumeMQTT>::getLogger()) {
    isSubscriber_ = true;
    maxQueueSize_ = 100;
    maxSegSize_ = ULLONG_MAX;
  }
  // Destructor
  ~ConsumeMQTT() override {
    MQTTClient_message *message;
    while (queue_.try_dequeue(message)) {
      MQTTClient_freeMessage(&message);
    }
  }
  // Processor Name
  static constexpr char const* ProcessorName = "ConsumeMQTT";
  // Supported Properties
  static core::Property MaxFlowSegSize;
  static core::Property QueueBufferMaxMessage;

  static core::Relationship Success;

  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(MQTTClient_message *message)
        : message_(message) {
    }
    MQTTClient_message *message_;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (message_->payloadlen < 0) {
        status_ = -1;
        return -1;
      }
      const auto len = stream->write(reinterpret_cast<uint8_t*>(message_->payload), gsl::narrow<size_t>(message_->payloadlen));
      if (io::isError(len)) {
        status_ = -1;
        return -1;
      }
      return gsl::narrow<int64_t>(len);
    }
    int status_ = 0;
  };

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;
  // OnTrigger method, implemented by NiFi ConsumeMQTT
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  // Initialize, over write by NiFi ConsumeMQTT
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

  std::shared_ptr<logging::Logger> logger_;
  std::mutex mutex_;
  uint64_t maxQueueSize_;
  uint64_t maxSegSize_;
  moodycamel::ConcurrentQueue<MQTTClient_message *> queue_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
