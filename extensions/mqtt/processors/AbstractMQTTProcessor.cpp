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
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

void AbstractMQTTProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*factory*/) {
  if (auto value = context->getProperty(BrokerURI)) {
    uri_ = std::move(*value);
    logger_->log_debug("AbstractMQTTProcessor: BrokerURI [%s]", uri_);
  }
  if (auto value = context->getProperty(ClientID)) {
    clientID_ = std::move(*value);
    logger_->log_debug("AbstractMQTTProcessor: ClientID [%s]", clientID_);
  }
  if (auto value = context->getProperty(Topic)) {
    topic_ = std::move(*value);
    logger_->log_debug("AbstractMQTTProcessor: Topic [%s]", topic_);
  }
  if (auto value = context->getProperty(Username)) {
    username_ = std::move(*value);
    logger_->log_debug("AbstractMQTTProcessor: UserName [%s]", username_);
  }
  if (auto value = context->getProperty(Password)) {
    password_ = std::move(*value);
    logger_->log_debug("AbstractMQTTProcessor: Password [%s]", password_);
  }

  if (const auto keep_alive_interval = context->getProperty<core::TimePeriodValue>(KeepAliveInterval)) {
    keep_alive_interval_ = std::chrono::duration_cast<std::chrono::seconds>(keep_alive_interval->getMilliseconds());
    logger_->log_debug("AbstractMQTTProcessor: KeepAliveInterval [%" PRId64 "] s", int64_t{keep_alive_interval_.count()});
  }

  if (const auto value = context->getProperty<uint64_t>(MaxFlowSegSize)) {
    max_seg_size_ = {*value};
    logger_->log_debug("PublishMQTT: max flow segment size [%" PRIu64 "]", max_seg_size_);
  }

  if (const auto connection_timeout = context->getProperty<core::TimePeriodValue>(ConnectionTimeout)) {
    connection_timeout_ = std::chrono::duration_cast<std::chrono::seconds>(connection_timeout->getMilliseconds());
    logger_->log_debug("AbstractMQTTProcessor: ConnectionTimeout [%" PRId64 "] s", int64_t{connection_timeout_.count()});
  }

  if (const auto value = context->getProperty<uint32_t>(QoS); value && (*value == MQTT_QOS_0 || *value == MQTT_QOS_1 || *value == MQTT_QOS_2)) {
    qos_ = {*value};
    logger_->log_debug("AbstractMQTTProcessor: QoS [%" PRIu32 "]", qos_);
  }

  if (const auto security_protocol = context->getProperty(SecurityProtocol)) {
    if (*security_protocol == MQTT_SECURITY_PROTOCOL_SSL) {
      sslOpts_ = MQTTAsync_SSLOptions_initializer;
      if (auto value = context->getProperty(SecurityCA)) {
        logger_->log_debug("AbstractMQTTProcessor: trustStore [%s]", *value);
        securityCA_ = std::move(*value);
        sslOpts_->trustStore = securityCA_.c_str();
      }
      if (auto value = context->getProperty(SecurityCert)) {
        logger_->log_debug("AbstractMQTTProcessor: keyStore [%s]", *value);
        securityCert_ = std::move(*value);
        sslOpts_->keyStore = securityCert_.c_str();
      }
      if (auto value = context->getProperty(SecurityPrivateKey)) {
        logger_->log_debug("AbstractMQTTProcessor: privateKey [%s]", *value);
        securityPrivateKey_ = std::move(*value);
        sslOpts_->privateKey = securityPrivateKey_.c_str();
      }
      if (auto value = context->getProperty(SecurityPrivateKeyPassword)) {
        logger_->log_debug("AbstractMQTTProcessor: privateKeyPassword [%s]", *value);
        securityPrivateKeyPassword_ = std::move(*value);
        sslOpts_->privateKeyPassword = securityPrivateKeyPassword_.c_str();
      }
    }
  }

  if (auto last_will_topic = context->getProperty(LastWillTopic); last_will_topic.has_value() && !last_will_topic->empty()) {
    last_will_ = MQTTAsync_willOptions_initializer;

    logger_->log_debug("AbstractMQTTProcessor: Last Will Topic [%s]", *last_will_topic);
    last_will_topic_ = std::move(*last_will_topic);
    last_will_->topicName = last_will_topic_.c_str();

    if (auto value = context->getProperty(LastWillMessage)) {
      logger_->log_debug("AbstractMQTTProcessor: Last Will Message [%s]", *value);
      last_will_message_ = std::move(*value);
      last_will_->message = last_will_message_.c_str();
    }

    if (const auto value = context->getProperty<uint32_t>(LastWillQoS); value && (*value == MQTT_QOS_0 || *value == MQTT_QOS_1 || *value == MQTT_QOS_2)) {
      logger_->log_debug("AbstractMQTTProcessor: Last Will QoS [%" PRIu32 "]", *value);
      last_will_qos_ = {*value};
      last_will_->qos = gsl::narrow<int>(last_will_qos_);
    }

    if (const auto value = context->getProperty<bool>(LastWillRetain)) {
      logger_->log_debug("AbstractMQTTProcessor: Last Will Retain [%d]", *value);
      last_will_retain_ = {*value};
      last_will_->retained = last_will_retain_;
    }
  }

  checkProperties();

  if (!client_) {
    if (MQTTAsync_create(&client_, uri_.c_str(), clientID_.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr) != MQTTASYNC_SUCCESS) {
      logger_->log_error("Creating MQTT client failed");
    }
  }
  if (client_) {
    if (MQTTAsync_setCallbacks(client_, this, connectionLost, msgReceived, nullptr) == MQTTASYNC_FAILURE) {
      logger_->log_error("Setting MQTT client callbacks failed");
      return;
    }
    // call reconnect to bootstrap
    reconnect();
  }
}

void AbstractMQTTProcessor::reconnect() {
  if (!client_) {
    logger_->log_error("MQTT client is not existing while trying to reconnect");
    return;
  }
  if (MQTTAsync_isConnected(client_)) {
    logger_->log_info("Already connected to %s, no need to reconnect", uri_);
    return;
  }
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  conn_opts.keepAliveInterval = gsl::narrow<int>(keep_alive_interval_.count());
  conn_opts.cleansession = getCleanSession();
  conn_opts.context = this;
  conn_opts.onSuccess = connectionSuccess;
  conn_opts.onFailure = connectionFailure;
  conn_opts.connectTimeout = gsl::narrow<int>(connection_timeout_.count());
  if (!username_.empty()) {
    conn_opts.username = username_.c_str();
    conn_opts.password = password_.c_str();
  }
  if (sslOpts_) {
    conn_opts.ssl = &*sslOpts_;
  }
  if (last_will_) {
    conn_opts.will = &*last_will_;
  }

  logger_->log_info("Reconnecting to %s", uri_);
  int ret = MQTTAsync_connect(client_, &conn_opts);
  if (ret != MQTTASYNC_SUCCESS) {
    logger_->log_error("Failed to reconnect to MQTT broker %s (%d)", uri_, ret);
  }
}

void AbstractMQTTProcessor::freeResources() {
  if (client_ && MQTTAsync_isConnected(client_)) {
    MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
    disconnect_options.context = this;
    disconnect_options.onSuccess = disconnectionSuccess;
    disconnect_options.onFailure = disconnectionFailure;
    disconnect_options.timeout = gsl::narrow<int>(std::chrono::milliseconds{connection_timeout_}.count());
    MQTTAsync_disconnect(client_, &disconnect_options);
  }
  if (client_) {
    MQTTAsync_destroy(&client_);
  }
}

}  // namespace org::apache::nifi::minifi::processors
