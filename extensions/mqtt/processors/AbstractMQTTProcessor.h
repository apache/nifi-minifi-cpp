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

#include <string>
#include <memory>
#include <vector>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "MQTTClient.h"

namespace org::apache::nifi::minifi::processors {

static constexpr const char* const MQTT_QOS_0 = "0";
static constexpr const char* const MQTT_QOS_1 = "1";
static constexpr const char* const MQTT_QOS_2 = "2";

static constexpr const char* const MQTT_SECURITY_PROTOCOL_SSL = "ssl";

class AbstractMQTTProcessor : public core::Processor {
 public:
  explicit AbstractMQTTProcessor(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }

  ~AbstractMQTTProcessor() override {
    if (isSubscriber_) {
      MQTTClient_unsubscribe(client_, topic_.c_str());
    }
    if (client_ && MQTTClient_isConnected(client_)) {
      MQTTClient_disconnect(client_, std::chrono::milliseconds{connectionTimeout_}.count());
    }
    if (client_)
      MQTTClient_destroy(&client_);
  }

  EXTENSIONAPI static const core::Property BrokerURI;
  EXTENSIONAPI static const core::Property ClientID;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property KeepLiveInterval;
  EXTENSIONAPI static const core::Property ConnectionTimeout;
  EXTENSIONAPI static const core::Property Topic;
  EXTENSIONAPI static const core::Property QoS;
  EXTENSIONAPI static const core::Property SecurityProtocol;
  EXTENSIONAPI static const core::Property SecurityCA;
  EXTENSIONAPI static const core::Property SecurityCert;
  EXTENSIONAPI static const core::Property SecurityPrivateKey;
  EXTENSIONAPI static const core::Property SecurityPrivateKeyPassword;

 EXTENSIONAPI static const auto properties() {
    return std::array{
      BrokerURI,
      ClientID,
      Username,
      Password,
      KeepLiveInterval,
      ConnectionTimeout,
      Topic,
      QoS,
      SecurityProtocol,
      SecurityCA,
      SecurityCert,
      SecurityPrivateKey,
      SecurityPrivateKeyPassword
    };
  }

 public:
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;

  // MQTT async callbacks
  static void msgDelivered(void *context, MQTTClient_deliveryToken dt) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->delivered_token_ = dt;
  }
  static int msgReceived(void *context, char *topicName, int /*topicLen*/, MQTTClient_message *message) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    if (processor->isSubscriber_) {
      if (!processor->enqueueReceiveMQTTMsg(message))
        MQTTClient_freeMessage(&message);
    } else {
      MQTTClient_freeMessage(&message);
    }
    MQTTClient_free(topicName);
    return 1;
  }
  static void connectionLost(void *context, char* /*cause*/) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->reconnect();
  }
  bool reconnect();
  virtual bool enqueueReceiveMQTTMsg(MQTTClient_message* /*message*/) {
    return false;
  }

 protected:
  MQTTClient client_ = nullptr;
  MQTTClient_deliveryToken delivered_token_ = 0;
  std::string uri_;
  std::string topic_;
  std::chrono::milliseconds keepAliveInterval_ = std::chrono::seconds(60);
  std::chrono::milliseconds connectionTimeout_ = std::chrono::seconds(30);
  int64_t qos_ = 0;
  std::string clientID_;
  std::string username_;
  std::string password_;
  bool isSubscriber_ = false;

 private:
  virtual bool getCleanSession() const = 0;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractMQTTProcessor>::getLogger();
  MQTTClient_SSLOptions sslOpts_ = MQTTClient_SSLOptions_initializer;
  bool sslEnabled_ = false;
  std::string securityCA_;
  std::string securityCert_;
  std::string securityPrivateKey_;
  std::string securityPrivateKeyPassword_;
};

}  // namespace org::apache::nifi::minifi::processors
