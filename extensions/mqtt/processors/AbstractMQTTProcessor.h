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
#include <shared_mutex>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Enum.h"
#include "MQTTAsync.h"

namespace org::apache::nifi::minifi::processors {

static constexpr const char* const MQTT_SECURITY_PROTOCOL_SSL = "ssl";

class AbstractMQTTProcessor : public core::Processor {
 public:
  explicit AbstractMQTTProcessor(std::string name, const utils::Identifier& uuid = {}, std::shared_ptr<core::ProcessorMetrics> metrics = {})
      : core::Processor(std::move(name), uuid, std::move(metrics)) {
  }

  ~AbstractMQTTProcessor() override {
    freeResources();
  }

  SMART_ENUM(MqttVersions,
    (V_3X_AUTO, "3.x AUTO"),
    (V_3_1_0, "3.1.0"),
    (V_3_1_1, "3.1.1"),
    (V_5_0, "5.0"));

  SMART_ENUM(MqttQoS,
    (LEVEL_0, "0"),
    (LEVEL_1, "1"),
    (LEVEL_2, "2"));

  EXTENSIONAPI static const core::Property BrokerURI;
  EXTENSIONAPI static const core::Property ClientID;
  EXTENSIONAPI static const core::Property QoS;
  EXTENSIONAPI static const core::Property MqttVersion;
  EXTENSIONAPI static const core::Property ConnectionTimeout;
  EXTENSIONAPI static const core::Property KeepAliveInterval;
  EXTENSIONAPI static const core::Property LastWillTopic;
  EXTENSIONAPI static const core::Property LastWillMessage;
  EXTENSIONAPI static const core::Property LastWillQoS;
  EXTENSIONAPI static const core::Property LastWillRetain;
  EXTENSIONAPI static const core::Property LastWillContentType;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;
  EXTENSIONAPI static const core::Property SecurityProtocol;
  EXTENSIONAPI static const core::Property SecurityCA;
  EXTENSIONAPI static const core::Property SecurityCert;
  EXTENSIONAPI static const core::Property SecurityPrivateKey;
  EXTENSIONAPI static const core::Property SecurityPrivateKeyPassword;


  static auto basicProperties() {
    return std::array{
      BrokerURI,
      ClientID,
      MqttVersion
    };
  }

  static auto advancedProperties() {
    return std::array{
      QoS,
      ConnectionTimeout,
      KeepAliveInterval,
      LastWillTopic,
      LastWillMessage,
      LastWillQoS,
      LastWillRetain,
      LastWillContentType,
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
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;

  void notifyStop() override {
    freeResources();
  }

 protected:
  struct MQTTMessageDeleter {
    void operator()(MQTTAsync_message* message) {
      MQTTAsync_freeMessage(&message);
    }
  };

  struct SmartMessage {
    std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter> contents;
    std::string topic;
  };

  // defined by Paho MQTT C library
  static constexpr int PAHO_MQTT_C_FAILURE_CODE = -9999999;
  static constexpr int MQTT_MAX_RECEIVE_MAXIMUM = 65535;

  /**
   * Connect to MQTT broker. Synchronously waits until connection succeeds or fails.
   */
  void reconnect();

  /**
   * Checks property consistency before connecting to broker
   */
  virtual void checkProperties() {
  }

  /**
   * Checks broker limits and supported features vs our desired features after connecting to broker
   */
  void checkBrokerLimits();
  virtual void checkBrokerLimitsImpl() = 0;

  // variables being used for a synchronous connection and disconnection
  std::shared_mutex client_mutex_;

  MQTTAsync client_ = nullptr;
  std::string uri_;
  std::chrono::seconds keep_alive_interval_{60};
  std::chrono::seconds connection_timeout_{10};
  MqttQoS qos_{MqttQoS::LEVEL_0};
  std::string clientID_;
  std::string username_;
  std::string password_;
  MqttVersions mqtt_version_{MqttVersions::V_3X_AUTO};

  // Supported operations
  std::optional<bool> retain_available_;
  std::optional<bool> wildcard_subscription_available_;
  std::optional<bool> shared_subscription_available_;

  std::optional<uint16_t> broker_topic_alias_maximum_;
  std::optional<uint16_t> broker_receive_maximum_;
  std::optional<uint8_t> maximum_qos_;
  std::optional<uint32_t> maximum_packet_size_;

  std::optional<std::chrono::seconds> maximum_session_expiry_interval_;
  std::optional<std::chrono::seconds> server_keep_alive_;

 private:
  using ConnectFinishedTask = std::packaged_task<void(MQTTAsync_successData*, MQTTAsync_successData5*, MQTTAsync_failureData*, MQTTAsync_failureData5*)>;

  /**
   * Initializes local MQTT client and connects to broker.
   */
  void initializeClient();

  /**
   * Calls disconnect() and releases local MQTT client
   */
  void freeResources();

  /**
   * Disconnect from MQTT broker. Synchronously waits until disconnection succeeds or fails.
   */
  void disconnect();

  virtual void readProperties(const std::shared_ptr<core::ProcessContext>& context) = 0;
  virtual void onTriggerImpl(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) = 0;
  virtual void startupClient() = 0;
  void setBrokerLimits(MQTTAsync_successData5* response);

  // MQTT static async callbacks, calling their non-static counterparts with context being pointer to "this"
  static void connectionLost(void *context, char* cause);
  static void connectionSuccess(void* context, MQTTAsync_successData* response);
  static void connectionSuccess5(void* context, MQTTAsync_successData5* response);
  static void connectionFailure(void* context, MQTTAsync_failureData* response);
  static void connectionFailure5(void* context, MQTTAsync_failureData5* response);
  static int msgReceived(void *context, char* topic_name, int topic_len, MQTTAsync_message* message);

  // MQTT async callback methods
  void onConnectionLost(char* cause);
  void onConnectFinished(MQTTAsync_successData* success_data, MQTTAsync_successData5* success_data_5, MQTTAsync_failureData* failure_data, MQTTAsync_failureData5* failure_data_5);
  void onDisconnectFinished(MQTTAsync_successData* success_data, MQTTAsync_successData5* success_data_5, MQTTAsync_failureData* failure_data, MQTTAsync_failureData5* failure_data_5);

  /**
   * Called if message is received. This is default implementation, to be overridden if subclass wants to use the message.
   * @param topic topic of message
   * @param message MQTT message
   */
  virtual void onMessageReceived(SmartMessage /*smartmessage*/) {
  }

  virtual bool getCleanSession() const = 0;
  virtual bool getCleanStart() const = 0;
  virtual std::chrono::seconds getSessionExpiryInterval() const = 0;
  MQTTAsync_connectOptions createConnectOptions(MQTTProperties& connect_properties, MQTTProperties& will_properties, ConnectFinishedTask& connect_finished_task);
  MQTTAsync_connectOptions createMqtt3ConnectOptions() const;
  MQTTAsync_connectOptions createMqtt5ConnectOptions(MQTTProperties& connect_properties, MQTTProperties& will_properties) const;
  virtual void setProcessorSpecificMqtt5ConnectOptions(MQTTProperties& /*connect_props*/) const {
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
  MqttQoS last_will_qos_{MqttQoS::LEVEL_0};
  bool last_will_retain_ = false;
  std::string last_will_content_type_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractMQTTProcessor>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
