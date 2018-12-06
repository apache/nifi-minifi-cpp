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
#ifndef __CONSUME_MQTT_H__
#define __CONSUME_MQTT_H__

#include <limits>
#include <deque>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "concurrentqueue.h"
#include "MQTTClient.h"
#include "AbstractMQTTProcessor.h"

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
  explicit ConsumeMQTT(std::string name, utils::Identifier uuid = utils::Identifier())
      : processors::AbstractMQTTProcessor(name, uuid),
        logger_(logging::LoggerFactory<ConsumeMQTT>::getLogger()) {
    isSubscriber_ = true;
    maxQueueSize_ = 100;
    maxSegSize_ = ULLONG_MAX;
  }
  // Destructor
  virtual ~ConsumeMQTT() {
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
  // Nest Callback Class for write stream
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(MQTTClient_message *message)
        : message_(message) {
      status_ = 0;
    }
    MQTTClient_message *message_;
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      int64_t len = stream->write(reinterpret_cast<uint8_t*>(message_->payload), message_->payloadlen);
      if (len < 0)
        status_ = -1;
      return len;
    }
    int status_;
  };

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  // OnTrigger method, implemented by NiFi ConsumeMQTT
  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
  // Initialize, over write by NiFi ConsumeMQTT
  virtual void initialize(void);
  virtual bool enqueueReceiveMQTTMsg(MQTTClient_message *message);

 protected:
  void getReceivedMQTTMsg(std::deque<MQTTClient_message *> &msg_queue) {
    MQTTClient_message *message;
    while (queue_.try_dequeue(message)) {
      msg_queue.push_back(message);
    }
  }

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::mutex mutex_;
  uint64_t maxQueueSize_;
  uint64_t maxSegSize_;
  moodycamel::ConcurrentQueue<MQTTClient_message *> queue_;
};

REGISTER_RESOURCE(ConsumeMQTT, "This Processor gets the contents of a FlowFile from a MQTT broker for a specified topic. The the payload of the MQTT message becomes content of a FlowFile");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
