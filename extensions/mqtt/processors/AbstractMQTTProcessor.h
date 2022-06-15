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
#include "MQTTAsync.h"

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
    freeResources();
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

  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& factory) override;

  void notifyStop() override;

  // MQTT async callbacks
  // TODO(amarkovics) this should only be in PublishMQTT
  static void msgDelivered(void *context, MQTTAsync_token dt) {
    // TODO(amarkovics) why do we set delivered_token_ at all?
    // TODO(amarkovics) this needs mutex because it's called on a separate thread
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    // TODO(amarkovics) can there be more than 1 message being delivered (asynchronously)?
    processor->delivered_token_ = dt;
  }

  // TODO(amarkovics) this should only be in ConsumeMQTT
  static int msgReceived(void *context, char *topicName, int /*topicLen*/, MQTTAsync_message* message) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onMessageReceived(message);
    MQTTAsync_free(topicName);
    return 1;
    // TODO(amarkovics) why always return with 1?
    // TODO(amarkovics) might need mutex
  }
  static void connectionLost(void *context, char* /*cause*/) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    // TODO(amarkovics) log cause
    // TODO(amarkovics) might need mutex
    processor->reconnect();
  }

  static void connectionFailure(void* context, MQTTAsync_failureData* response) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onConnectionFailure(response);
  }

  static void connectionSuccess(void* context, MQTTAsync_successData* response) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onConnectionSuccess(response);
  }

  bool reconnect();

 protected:
  MQTTAsync client_ = nullptr;
  MQTTAsync_token delivered_token_ = 0;
  std::string uri_;
  std::string topic_;
  std::chrono::milliseconds keepAliveInterval_ = std::chrono::seconds(60);
  std::chrono::milliseconds connectionTimeout_ = std::chrono::seconds(30);
  int64_t qos_ = 0;
  std::string clientID_;
  std::string username_;
  std::string password_;

 private:
  virtual bool getCleanSession() const = 0;
  virtual void onMessageReceived(MQTTAsync_message*) = 0;
  virtual bool startupClient() = 0;

  void freeResources();

  void onConnectionFailure(MQTTAsync_failureData* response) {
    logger_->log_error("Failed to connect to MQTT broker %s (%d)", uri_, response->code);
    if (response->message != nullptr) {
      logger_->log_error("Detailed reason for connection failure: %s", response->message);
    }
  }

  void onConnectionSuccess(MQTTAsync_successData* /*response*/) {
    startupClient();
  }

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractMQTTProcessor>::getLogger();
  MQTTAsync_SSLOptions sslOpts_ = MQTTAsync_SSLOptions_initializer;
  bool sslEnabled_ = false;
  std::string securityCA_;
  std::string securityCert_;
  std::string securityPrivateKey_;
  std::string securityPrivateKeyPassword_;
};

}  // namespace org::apache::nifi::minifi::processors
