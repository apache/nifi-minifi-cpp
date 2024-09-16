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

#include "core/PropertyDefinition.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Enum.h"
#include "MQTTAsync.h"

namespace org::apache::nifi::minifi::processors::mqtt {
enum class MqttVersions {
  V_3X_AUTO,
  V_3_1_0,
  V_3_1_1,
  V_5_0
};

enum class MqttQoS : uint8_t {
  LEVEL_0,
  LEVEL_1,
  LEVEL_2
};
}  // namespace org::apache::nifi::minifi::processors::mqtt

namespace magic_enum::customize {
using MqttVersions = org::apache::nifi::minifi::processors::mqtt::MqttVersions;
using MqttQoS = org::apache::nifi::minifi::processors::mqtt::MqttQoS;

template <>
constexpr customize_t enum_name<MqttVersions>(MqttVersions value) noexcept {
  switch (value) {
    case MqttVersions::V_3X_AUTO:
      return "3.x AUTO";
    case MqttVersions::V_3_1_0:
      return "3.1.0";
    case MqttVersions::V_3_1_1:
      return "3.1.1";
    case MqttVersions::V_5_0:
      return "5.0";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<MqttQoS>(MqttQoS value) noexcept {
  switch (value) {
    case MqttQoS::LEVEL_0:
      return "0";
    case MqttQoS::LEVEL_1:
      return "1";
    case MqttQoS::LEVEL_2:
      return "2";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::processors {

static constexpr const char* const MQTT_SECURITY_PROTOCOL_SSL = "ssl";

class AbstractMQTTProcessor : public core::ProcessorImpl {
 public:
  explicit AbstractMQTTProcessor(std::string_view name, const utils::Identifier& uuid = {}, std::shared_ptr<core::ProcessorMetrics> metrics = {})
      : core::ProcessorImpl(name, uuid, std::move(metrics)) {
  }

  ~AbstractMQTTProcessor() override {
    freeResources();
  }

  EXTENSIONAPI static constexpr auto BrokerURI = core::PropertyDefinitionBuilder<>::createProperty("Broker URI")
      .withDescription("The URI to use to connect to the MQTT broker")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ClientID = core::PropertyDefinitionBuilder<>::createProperty("Client ID")
      .withDescription("MQTT client ID to use. WARNING: Must not be empty when using MQTT 3.1.0!")
      .build();
  EXTENSIONAPI static constexpr auto QoS = core::PropertyDefinitionBuilder<magic_enum::enum_count<mqtt::MqttQoS>()>::createProperty("Quality of Service")
      .withDescription("The Quality of Service (QoS) of messages.")
      .withDefaultValue(magic_enum::enum_name(mqtt::MqttQoS::LEVEL_0))
      .withAllowedValues(magic_enum::enum_names<mqtt::MqttQoS>())
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MqttVersion = core::PropertyDefinitionBuilder<magic_enum::enum_count<mqtt::MqttVersions>()>::createProperty("MQTT Version")
      .withDescription("The MQTT specification version when connecting to the broker.")
      .withDefaultValue(magic_enum::enum_name(mqtt::MqttVersions::V_3X_AUTO))
      .withAllowedValues(magic_enum::enum_names<mqtt::MqttVersions>())
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ConnectionTimeout = core::PropertyDefinitionBuilder<>::createProperty("Connection Timeout")
      .withDescription("Maximum time interval the client will wait for the network connection to the MQTT broker")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("10 sec")
      .build();
  EXTENSIONAPI static constexpr auto KeepAliveInterval = core::PropertyDefinitionBuilder<>::createProperty("Keep Alive Interval")
      .withDescription("Defines the maximum time interval between messages sent or received")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("60 sec")
      .build();
  EXTENSIONAPI static constexpr auto LastWillTopic = core::PropertyDefinitionBuilder<>::createProperty("Last Will Topic")
      .withDescription("The topic to send the client's Last Will to. If the Last Will topic is not set then a Last Will will not be sent")
      .build();
  EXTENSIONAPI static constexpr auto LastWillMessage = core::PropertyDefinitionBuilder<>::createProperty("Last Will Message")
      .withDescription("The message to send as the client's Last Will. If the Last Will Message is empty, Last Will will be deleted from the broker")
      .build();
  EXTENSIONAPI static constexpr auto LastWillQoS = core::PropertyDefinitionBuilder<magic_enum::enum_count<mqtt::MqttQoS>()>::createProperty("Last Will QoS")
      .withDescription("The Quality of Service (QoS) to send the last will with.")
      .withDefaultValue(magic_enum::enum_name(mqtt::MqttQoS::LEVEL_0))
      .withAllowedValues(magic_enum::enum_names<mqtt::MqttQoS>())
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto LastWillRetain = core::PropertyDefinitionBuilder<>::createProperty("Last Will Retain")
      .withDescription("Whether to retain the client's Last Will")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto LastWillContentType = core::PropertyDefinitionBuilder<>::createProperty("Last Will Content Type")
      .withDescription("Content type of the client's Last Will. MQTT 5.x only.")
      .build();
  EXTENSIONAPI static constexpr auto Username = core::PropertyDefinitionBuilder<>::createProperty("Username")
      .withDescription("Username to use when connecting to the broker")
      .build();
  EXTENSIONAPI static constexpr auto Password = core::PropertyDefinitionBuilder<>::createProperty("Password")
      .withDescription("Password to use when connecting to the broker")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto SecurityProtocol = core::PropertyDefinitionBuilder<>::createProperty("Security Protocol")
      .withDescription("Protocol used to communicate with brokers")
      .build();
  EXTENSIONAPI static constexpr auto SecurityCA = core::PropertyDefinitionBuilder<>::createProperty("Security CA")
      .withDescription("File or directory path to CA certificate(s) for verifying the broker's key")
      .build();
  EXTENSIONAPI static constexpr auto SecurityCert = core::PropertyDefinitionBuilder<>::createProperty("Security Cert")
      .withDescription("Path to client's public key (PEM) used for authentication")
      .build();
  EXTENSIONAPI static constexpr auto SecurityPrivateKey = core::PropertyDefinitionBuilder<>::createProperty("Security Private Key")
      .withDescription("Path to client's private key (PEM) used for authentication")
      .build();
  EXTENSIONAPI static constexpr auto SecurityPrivateKeyPassword = core::PropertyDefinitionBuilder<>::createProperty("Security Pass Phrase")
      .withDescription("Private key passphrase")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto BasicProperties = std::to_array<core::PropertyReference>({
      BrokerURI,
      ClientID,
      MqttVersion
  });
  EXTENSIONAPI static constexpr auto AdvancedProperties = std::to_array<core::PropertyReference>({
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
  });

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

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
  static constexpr std::string_view MQTT_MAX_RECEIVE_MAXIMUM_STR = "65535";

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
  mqtt::MqttQoS qos_{mqtt::MqttQoS::LEVEL_0};
  std::string clientID_;
  std::string username_;
  std::string password_;
  mqtt::MqttVersions mqtt_version_{mqtt::MqttVersions::V_3X_AUTO};

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

  virtual void readProperties(core::ProcessContext& context) = 0;
  virtual void onTriggerImpl(core::ProcessContext& context, core::ProcessSession& session) = 0;
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
  mqtt::MqttQoS last_will_qos_{mqtt::MqttQoS::LEVEL_0};
  bool last_will_retain_ = false;
  std::string last_will_content_type_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AbstractMQTTProcessor>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
