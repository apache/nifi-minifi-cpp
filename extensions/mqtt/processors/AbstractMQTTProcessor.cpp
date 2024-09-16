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
#include "AbstractMQTTProcessor.h"
#include <memory>
#include <string>
#include <utility>

#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

void AbstractMQTTProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  uri_ = utils::parseProperty(context, BrokerURI);
  mqtt_version_ = utils::parseEnumProperty<mqtt::MqttVersions>(context, MqttVersion);

  if (auto value = context.getProperty(ClientID)) {
    clientID_ = std::move(*value);
  } else if (mqtt_version_ == mqtt::MqttVersions::V_3_1_0) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "MQTT 3.1.0 specification does not support empty client IDs");
  }

  if (auto value = context.getProperty(Username)) {
    username_ = std::move(*value);
  }
  logger_->log_debug("AbstractMQTTProcessor: Username [{}]", username_);

  if (auto value = context.getProperty(Password)) {
    password_ = std::move(*value);
  }
  logger_->log_debug("AbstractMQTTProcessor: Password [{}]", password_);

  keep_alive_interval_ = std::chrono::duration_cast<std::chrono::seconds>(utils::parseMsProperty(context, KeepAliveInterval));
  logger_->log_debug("AbstractMQTTProcessor: KeepAliveInterval [{}] s", int64_t{keep_alive_interval_.count()});

  connection_timeout_ = std::chrono::duration_cast<std::chrono::seconds>(utils::parseMsProperty(context, ConnectionTimeout));
  logger_->log_debug("AbstractMQTTProcessor: ConnectionTimeout [{}] s", int64_t{connection_timeout_.count()});

  qos_ = utils::parseEnumProperty<mqtt::MqttQoS>(context, QoS);
  logger_->log_debug("AbstractMQTTProcessor: QoS [{}]", magic_enum::enum_name(qos_));

  if (const auto security_protocol = context.getProperty(SecurityProtocol)) {
    if (*security_protocol == MQTT_SECURITY_PROTOCOL_SSL) {
      sslOpts_ = MQTTAsync_SSLOptions_initializer;
      if (auto value = context.getProperty(SecurityCA)) {
        logger_->log_debug("AbstractMQTTProcessor: trustStore [{}]", *value);
        securityCA_ = std::move(*value);
        sslOpts_->trustStore = securityCA_.c_str();
      }
      if (auto value = context.getProperty(SecurityCert)) {
        logger_->log_debug("AbstractMQTTProcessor: keyStore [{}]", *value);
        securityCert_ = std::move(*value);
        sslOpts_->keyStore = securityCert_.c_str();
      }
      if (auto value = context.getProperty(SecurityPrivateKey)) {
        logger_->log_debug("AbstractMQTTProcessor: privateKey [{}]", *value);
        securityPrivateKey_ = std::move(*value);
        sslOpts_->privateKey = securityPrivateKey_.c_str();
      }
      if (auto value = context.getProperty(SecurityPrivateKeyPassword)) {
        logger_->log_debug("AbstractMQTTProcessor: privateKeyPassword [{}]", *value);
        securityPrivateKeyPassword_ = std::move(*value);
        sslOpts_->privateKeyPassword = securityPrivateKeyPassword_.c_str();
      }
    }
  }

  if (auto last_will_topic = context.getProperty(LastWillTopic); last_will_topic.has_value() && !last_will_topic->empty()) {
    last_will_ = MQTTAsync_willOptions_initializer;

    logger_->log_debug("AbstractMQTTProcessor: Last Will Topic [{}]", *last_will_topic);
    last_will_topic_ = std::move(*last_will_topic);
    last_will_->topicName = last_will_topic_.c_str();

    if (auto value = context.getProperty(LastWillMessage)) {
      logger_->log_debug("AbstractMQTTProcessor: Last Will Message [{}]", *value);
      last_will_message_ = std::move(*value);
      last_will_->message = last_will_message_.c_str();
    }

    last_will_qos_ = utils::parseEnumProperty<mqtt::MqttQoS>(context, LastWillQoS);
    logger_->log_debug("AbstractMQTTProcessor: Last Will QoS [{}]", magic_enum::enum_name(last_will_qos_));
    last_will_->qos = static_cast<int>(last_will_qos_);

    last_will_retain_ = utils::parseBoolProperty(context, LastWillRetain);
    last_will_->retained = last_will_retain_;

    if (auto value = context.getProperty(LastWillContentType)) {
      logger_->log_debug("AbstractMQTTProcessor: Last Will Content Type [{}]", *value);
      last_will_content_type_ = std::move(*value);
    }
  }

  readProperties(context);
  checkProperties();
  initializeClient();
}

void AbstractMQTTProcessor::initializeClient() {
  // write lock
  std::lock_guard client_lock{client_mutex_};

  if (!client_) {
    MQTTAsync_createOptions options = MQTTAsync_createOptions_initializer;
    if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
      options.MQTTVersion = MQTTVERSION_5;
    }
    if (MQTTAsync_createWithOptions(&client_, uri_.c_str(), clientID_.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr, &options) != MQTTASYNC_SUCCESS) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Creating MQTT client failed");
    }
  }
  if (client_) {
    if (MQTTAsync_setCallbacks(client_, this, connectionLost, msgReceived, nullptr) == MQTTASYNC_FAILURE) {
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Setting MQTT client callbacks failed");
    }
    // call reconnect to bootstrap
    reconnect();
  }
}

void AbstractMQTTProcessor::reconnect() {
  if (!client_) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "MQTT client is not existing while trying to reconnect");
  }
  if (MQTTAsync_isConnected(client_)) {
    logger_->log_debug("Already connected to {}, no need to reconnect", uri_);
    return;
  }

  MQTTProperties connect_properties = MQTTProperties_initializer;
  MQTTProperties will_properties = MQTTProperties_initializer;

  ConnectFinishedTask connect_finished_task(
          [this] (MQTTAsync_successData* success_data, MQTTAsync_successData5* success_data_5, MQTTAsync_failureData* failure_data, MQTTAsync_failureData5* failure_data_5) {
            onConnectFinished(success_data, success_data_5, failure_data, failure_data_5);
          });

  const MQTTAsync_connectOptions connect_options = createConnectOptions(connect_properties, will_properties, connect_finished_task);

  logger_->log_info("Reconnecting to {}", uri_);
  if (MQTTAsync_isConnected(client_)) {
    logger_->log_debug("Already connected to {}, no need to reconnect", uri_);
    return;
  }

  const int ret = MQTTAsync_connect(client_, &connect_options);
  MQTTProperties_free(&connect_properties);
  if (ret != MQTTASYNC_SUCCESS) {
    logger_->log_error("MQTTAsync_connect failed to MQTT broker {} with error code [{}]", uri_, ret);
    return;
  }

  // wait until connection succeeds or fails
  connect_finished_task.get_future().get();
}

MQTTAsync_connectOptions AbstractMQTTProcessor::createConnectOptions(MQTTProperties& connect_properties, MQTTProperties& will_properties, ConnectFinishedTask& connect_finished_task) {
  MQTTAsync_connectOptions connect_options = [this, &connect_properties, &will_properties] {
    if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
      return createMqtt5ConnectOptions(connect_properties, will_properties);
    } else {
      return createMqtt3ConnectOptions();
    }
  }();

  connect_options.context = &connect_finished_task;
  connect_options.connectTimeout = gsl::narrow<int>(connection_timeout_.count());
  connect_options.keepAliveInterval = gsl::narrow<int>(keep_alive_interval_.count());
  if (!username_.empty()) {
    connect_options.username = username_.c_str();
    connect_options.password = password_.c_str();
  }
  if (sslOpts_) {
    connect_options.ssl = &*sslOpts_;
  }
  if (last_will_) {
    connect_options.will = &*last_will_;
  }

  return connect_options;
}

MQTTAsync_connectOptions AbstractMQTTProcessor::createMqtt3ConnectOptions() const {
  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer;
  connect_options.onSuccess = connectionSuccess;
  connect_options.onFailure = connectionFailure;
  connect_options.cleansession = getCleanSession();

  if (mqtt_version_ == mqtt::MqttVersions::V_3_1_0) {
    connect_options.MQTTVersion = MQTTVERSION_3_1;
  } else if (mqtt_version_ == mqtt::MqttVersions::V_3_1_1) {
    connect_options.MQTTVersion = MQTTVERSION_3_1_1;
  }

  return connect_options;
}

MQTTAsync_connectOptions AbstractMQTTProcessor::createMqtt5ConnectOptions(MQTTProperties& connect_properties, MQTTProperties& will_properties) const {
  MQTTAsync_connectOptions connect_options = MQTTAsync_connectOptions_initializer5;
  connect_options.onSuccess5 = connectionSuccess5;
  connect_options.onFailure5 = connectionFailure5;
  connect_options.connectProperties = &connect_properties;

  connect_options.cleanstart = getCleanStart();

  {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL;
    property.value.integer4 = gsl::narrow<unsigned int>(getSessionExpiryInterval().count());  // NOLINT(cppcoreguidelines-pro-type-union-access)
    MQTTProperties_add(&connect_properties, &property);
  }

  if (!last_will_content_type_.empty()) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_CONTENT_TYPE;
    property.value.data.len = gsl::narrow<int>(last_will_content_type_.length());  // NOLINT(cppcoreguidelines-pro-type-union-access)
    property.value.data.data = const_cast<char*>(last_will_content_type_.data());  // NOLINT(cppcoreguidelines-pro-type-union-access)
    MQTTProperties_add(&will_properties, &property);
  }

  connect_options.willProperties = &will_properties;

  setProcessorSpecificMqtt5ConnectOptions(connect_properties);

  return connect_options;
}

void AbstractMQTTProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  std::shared_lock client_lock{client_mutex_};
  if (client_ == nullptr) {
    logger_->log_debug("Null-op in onTrigger, processor is shutting down.");
    return;
  }

  reconnect();

  if (!MQTTAsync_isConnected(client_)) {
    logger_->log_error("Could not work with MQTT broker because disconnected to {}", uri_);
    yield();
    return;
  }

  onTriggerImpl(context, session);
}

void AbstractMQTTProcessor::freeResources() {
  // write lock
  std::lock_guard client_lock{client_mutex_};

  if (!client_) {
    return;
  }

  disconnect();

  MQTTAsync_destroy(&client_);
}

void AbstractMQTTProcessor::disconnect() {
  if (!MQTTAsync_isConnected(client_)) {
    return;
  }

  MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
  ConnectFinishedTask disconnect_finished_task(
          [this] (MQTTAsync_successData* success_data, MQTTAsync_successData5* success_data_5, MQTTAsync_failureData* failure_data, MQTTAsync_failureData5* failure_data_5) {
            onDisconnectFinished(success_data, success_data_5, failure_data, failure_data_5);
          });
  disconnect_options.context = &disconnect_finished_task;

  if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
    disconnect_options.onSuccess5 = connectionSuccess5;
    disconnect_options.onFailure5 = connectionFailure5;
  } else {
    disconnect_options.onSuccess = connectionSuccess;
    disconnect_options.onFailure = connectionFailure;
  }

  disconnect_options.timeout = gsl::narrow<int>(std::chrono::milliseconds{connection_timeout_}.count());

  const int ret = MQTTAsync_disconnect(client_, &disconnect_options);
  if (ret != MQTTASYNC_SUCCESS) {
    logger_->log_error("MQTTAsync_disconnect failed to MQTT broker {} with error code [{}]", uri_, ret);
    return;
  }

  // wait until connection succeeds or fails
  disconnect_finished_task.get_future().get();
}

void AbstractMQTTProcessor::setBrokerLimits(MQTTAsync_successData5* response) {
  auto readProperty = [response] (MQTTPropertyCodes property_code, auto& out_var) {
    const int value = MQTTProperties_getNumericValue(&response->properties, property_code);
    if (value != PAHO_MQTT_C_FAILURE_CODE) {
      if constexpr (std::is_same_v<decltype(out_var), std::optional<std::chrono::seconds>&>) {
        out_var = std::chrono::seconds(value);
      } else {
        out_var = gsl::narrow<typename std::remove_reference_t<decltype(out_var)>::value_type>(value);
      }
    } else {
      out_var.reset();
    }
  };

  readProperty(MQTTPROPERTY_CODE_RETAIN_AVAILABLE, retain_available_);
  readProperty(MQTTPROPERTY_CODE_WILDCARD_SUBSCRIPTION_AVAILABLE, wildcard_subscription_available_);
  readProperty(MQTTPROPERTY_CODE_SHARED_SUBSCRIPTION_AVAILABLE, shared_subscription_available_);

  readProperty(MQTTPROPERTY_CODE_TOPIC_ALIAS_MAXIMUM, broker_topic_alias_maximum_);
  readProperty(MQTTPROPERTY_CODE_RECEIVE_MAXIMUM, broker_receive_maximum_);
  readProperty(MQTTPROPERTY_CODE_MAXIMUM_QOS, maximum_qos_);
  readProperty(MQTTPROPERTY_CODE_MAXIMUM_PACKET_SIZE, maximum_packet_size_);

  readProperty(MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL, maximum_session_expiry_interval_);
  readProperty(MQTTPROPERTY_CODE_SERVER_KEEP_ALIVE, server_keep_alive_);
}

void AbstractMQTTProcessor::checkBrokerLimits() {
  try {
    if (server_keep_alive_.has_value() && server_keep_alive_ < keep_alive_interval_) {
      std::ostringstream os;
      os << "Set Keep Alive Interval (" << keep_alive_interval_.count() << " s) is longer than the maximum supported by the broker (" << server_keep_alive_->count() << " s)";
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, os.str());
    }

    if (maximum_qos_.has_value() && static_cast<uint8_t>(qos_) > maximum_qos_) {
      std::ostringstream os;
      os << "Set QoS (" << static_cast<uint8_t>(qos_) << ") is higher than the maximum supported by the broker (" << *maximum_qos_ << ")";
      throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, os.str());
    }

    checkBrokerLimitsImpl();
  }
  catch (...) {
    disconnect();
    throw;
  }
}

void AbstractMQTTProcessor::connectionLost(void *context, char* cause) {
  auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
  processor->onConnectionLost(cause);
}


void AbstractMQTTProcessor::connectionSuccess(void* context, MQTTAsync_successData* response) {
  auto* task = reinterpret_cast<ConnectFinishedTask*>(context);
  (*task)(response, nullptr, nullptr, nullptr);
}

void AbstractMQTTProcessor::connectionSuccess5(void* context, MQTTAsync_successData5* response) {
  auto* task = reinterpret_cast<ConnectFinishedTask*>(context);
  (*task)(nullptr, response, nullptr, nullptr);
}

void AbstractMQTTProcessor::connectionFailure(void* context, MQTTAsync_failureData* response) {
  auto* task = reinterpret_cast<ConnectFinishedTask*>(context);
  (*task)(nullptr, nullptr, response, nullptr);
}

void AbstractMQTTProcessor::connectionFailure5(void* context, MQTTAsync_failureData5* response) {
  auto* task = reinterpret_cast<ConnectFinishedTask*>(context);
  (*task)(nullptr, nullptr, nullptr, response);
}

int AbstractMQTTProcessor::msgReceived(void *context, char* topic_name, int topic_len, MQTTAsync_message* message) {
  auto* processor = reinterpret_cast<AbstractMQTTProcessor*>(context);
  processor->onMessageReceived(SmartMessage{std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter>(message), std::string(topic_name, topic_len)});
  MQTTAsync_free(topic_name);
  return 1;
}

void AbstractMQTTProcessor::onConnectionLost(char* cause) {
  logger_->log_error("Connection lost to MQTT broker {}", uri_);
  if (cause != nullptr) {
    logger_->log_error("Cause for connection loss: {}", cause);
  }
}

void AbstractMQTTProcessor::onConnectFinished(MQTTAsync_successData* success_data, MQTTAsync_successData5* success_data_5,
                                              MQTTAsync_failureData* failure_data, MQTTAsync_failureData5* failure_data_5) {
  if (success_data) {
    logger_->log_info("Successfully connected to MQTT broker {}", uri_);
    startupClient();
    return;
  }

  if (success_data_5) {
    logger_->log_info("Successfully connected to MQTT broker {}", uri_);
    logger_->log_info("Reason code for connection success: {}: {}", magic_enum::enum_underlying(success_data_5->reasonCode), MQTTReasonCode_toString(success_data_5->reasonCode));
    setBrokerLimits(success_data_5);
    checkBrokerLimits();
    startupClient();
    return;
  }

  if (failure_data) {
    logger_->log_error("Connection failed to MQTT broker {} ({})", uri_, failure_data->code);
    if (failure_data->message != nullptr) {
      logger_->log_error("Detailed reason for connection failure: {}", failure_data->message);
    }
    return;
  }

  if (failure_data_5) {
    logger_->log_error("Connection failed to MQTT broker {} ({})", uri_, failure_data_5->code);
    if (failure_data_5->message != nullptr) {
      logger_->log_error("Detailed reason for connection failure: {}", failure_data_5->message);
    }
    logger_->log_error("Reason code for connection failure: {}: {}", magic_enum::enum_underlying(failure_data_5->reasonCode), MQTTReasonCode_toString(failure_data_5->reasonCode));
  }
}

void AbstractMQTTProcessor::onDisconnectFinished(MQTTAsync_successData* success_data, MQTTAsync_successData5* success_data_5,
                                                 MQTTAsync_failureData* failure_data, MQTTAsync_failureData5* failure_data_5) {
  if (success_data) {
    logger_->log_info("Successfully disconnected from MQTT broker {}", uri_);
    return;
  }

  if (success_data_5) {
    logger_->log_info("Successfully disconnected from MQTT broker {}", uri_);
    logger_->log_info("Reason code for disconnection success: {}: {}", magic_enum::enum_underlying(success_data_5->reasonCode), MQTTReasonCode_toString(success_data_5->reasonCode));
    return;
  }

  if (failure_data) {
    logger_->log_error("Disconnection failed from MQTT broker {} ({})", uri_, failure_data->code);
    if (failure_data->message != nullptr) {
      logger_->log_error("Detailed reason for disconnection failure: {}", failure_data->message);
    }
    return;
  }

  if (failure_data_5) {
    logger_->log_error("Disconnection failed from MQTT broker {} ({})", uri_, failure_data_5->code);
    if (failure_data_5->message != nullptr) {
      logger_->log_error("Detailed reason for disconnection failure: {}", failure_data_5->message);
    }
    logger_->log_error("Reason code for disconnection failure: {}: {}", magic_enum::enum_underlying(failure_data_5->reasonCode), MQTTReasonCode_toString(failure_data_5->reasonCode));
  }
}

}  // namespace org::apache::nifi::minifi::processors
