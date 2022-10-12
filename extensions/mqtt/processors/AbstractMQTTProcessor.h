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
#include <utility>
#include <vector>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "MQTTAsync.h"

namespace org::apache::nifi::minifi::processors {

static constexpr uint8_t MQTT_QOS_0 = 0;
static constexpr uint8_t MQTT_QOS_1 = 1;
static constexpr uint8_t MQTT_QOS_2 = 2;

static constexpr const char* const MQTT_SECURITY_PROTOCOL_SSL = "ssl";

class AbstractMQTTProcessor : public core::Processor {
 public:
  explicit AbstractMQTTProcessor(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  ~AbstractMQTTProcessor() override {
    freeResources();
  }

  EXTENSIONAPI static const core::Property BrokerURI;
  EXTENSIONAPI static const core::Property ClientID;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property KeepAliveInterval;
  EXTENSIONAPI static const core::Property MaxFlowSegSize;
  EXTENSIONAPI static const core::Property ConnectionTimeout;
  EXTENSIONAPI static const core::Property Topic;
  EXTENSIONAPI static const core::Property QoS;
  EXTENSIONAPI static const core::Property SecurityProtocol;
  EXTENSIONAPI static const core::Property SecurityCA;
  EXTENSIONAPI static const core::Property SecurityCert;
  EXTENSIONAPI static const core::Property SecurityPrivateKey;
  EXTENSIONAPI static const core::Property SecurityPrivateKeyPassword;
  EXTENSIONAPI static const core::Property LastWillTopic;
  EXTENSIONAPI static const core::Property LastWillMessage;
  EXTENSIONAPI static const core::Property LastWillQoS;
  EXTENSIONAPI static const core::Property LastWillRetain;

  EXTENSIONAPI static auto properties() {
    return std::array{
            BrokerURI,
            Topic,
            ClientID,
            QoS,
            ConnectionTimeout,
            KeepAliveInterval,
            MaxFlowSegSize,
            LastWillTopic,
            LastWillMessage,
            LastWillQoS,
            LastWillRetain,
            Username,
            Password,
            SecurityProtocol,
            SecurityCA,
            SecurityCert,
            SecurityPrivateKey,
            SecurityPrivateKeyPassword
    };
  }

  void onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& factory) override;

  void notifyStop() override {
    freeResources();
  }

 protected:
  void reconnect();

  MQTTAsync client_ = nullptr;
  std::string uri_;
  std::string topic_;
  std::chrono::seconds keep_alive_interval_{60};
  uint64_t max_seg_size_ = std::numeric_limits<uint64_t>::max();
  std::chrono::seconds connection_timeout_{30};
  uint32_t qos_ = MQTT_QOS_1;
  std::string clientID_;
  std::string username_;
  std::string password_;

 private:
  // MQTT async callback
  static int msgReceived(void *context, char* topic_name, int topic_len, MQTTAsync_message* message) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onMessageReceived(topic_name, topic_len, message);
    return 1;
  }

  // MQTT async callback
  static void connectionLost(void *context, char* cause) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onConnectionLost(cause);
  }

  // MQTT async callback
  static void connectionSuccess(void* context, MQTTAsync_successData* response) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onConnectionSuccess(response);
  }

  // MQTT async callback
  static void connectionFailure(void* context, MQTTAsync_failureData* response) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onConnectionFailure(response);
  }

  // MQTT async callback
  static void disconnectionSuccess(void* context, MQTTAsync_successData* response) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onDisconnectionSuccess(response);
  }

  // MQTT async callback
  static void disconnectionFailure(void* context, MQTTAsync_failureData* response) {
    auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
    processor->onDisconnectionFailure(response);
  }

  virtual void onMessageReceived(char* topic_name, int /*topic_len*/, MQTTAsync_message* message) {
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic_name);
  }

  void onConnectionLost(char* cause) {
    logger_->log_error("Connection lost to MQTT broker %s", uri_);
    if (cause != nullptr) {
      logger_->log_error("Cause for connection loss: %s", cause);
    }
  }

  void onConnectionSuccess(MQTTAsync_successData* /*response*/) {
    logger_->log_info("Successfully connected to MQTT broker %s", uri_);
    startupClient();
  }

  void onConnectionFailure(MQTTAsync_failureData* response) {
    logger_->log_error("Connection failed to MQTT broker %s (%d)", uri_, response->code);
    if (response->message != nullptr) {
      logger_->log_error("Detailed reason for connection failure: %s", response->message);
    }
  }

  void onDisconnectionSuccess(MQTTAsync_successData* /*response*/) {
    logger_->log_info("Successfully disconnected from MQTT broker %s", uri_);
  }

  void onDisconnectionFailure(MQTTAsync_failureData* response) {
    logger_->log_error("Disconnection failed from MQTT broker %s (%d)", uri_, response->code);
    if (response->message != nullptr) {
      logger_->log_error("Detailed reason for disconnection failure: %s", response->message);
    }
  }

  virtual bool getCleanSession() const = 0;
  virtual bool startupClient() = 0;

  void freeResources();

  /**
   * Checks property consistency before connecting to broker
   */
  virtual void checkProperties() {
  }

  // SSL
  std::optional<MQTTAsync_SSLOptions> sslOpts_;
  std::string securityCA_;
  std::string securityCert_;
  std::string securityPrivateKey_;
  std::string securityPrivateKeyPassword_;

  // Last Will
  std::optional<MQTTAsync_willOptions> last_will_;
  std::string last_will_topic_;
  std::string last_will_message_;
  uint32_t last_will_qos_ = MQTT_QOS_1;
  bool last_will_retain_ = false;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractMQTTProcessor>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
