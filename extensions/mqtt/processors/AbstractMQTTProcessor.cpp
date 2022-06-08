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
#include <cstdio>
#include <memory>
#include <string>
#include <cinttypes>
#include <vector>

#include "utils/StringUtils.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

core::Property AbstractMQTTProcessor::BrokerURI(
  core::PropertyBuilder::createProperty("Broker URI")->
    withDescription("The URI to use to connect to the MQTT broker")->
    isRequired(true)->
    build());

core::Property AbstractMQTTProcessor::ClientID("Client ID", "MQTT client ID to use", "");

core::Property AbstractMQTTProcessor::Topic(
  core::PropertyBuilder::createProperty("Topic")->
    withDescription("The topic to publish the message to")->
    isRequired(true)->
    build());

core::Property AbstractMQTTProcessor::QoS("Quality of Service", "The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2'", MQTT_QOS_0);
core::Property AbstractMQTTProcessor::KeepLiveInterval("Keep Alive Interval", "Defines the maximum time interval between messages sent or received", "60 sec");
core::Property AbstractMQTTProcessor::ConnectionTimeout("Connection Timeout", "Maximum time interval the client will wait for the network connection to the MQTT server", "30 sec");
core::Property AbstractMQTTProcessor::Username("Username", "Username to use when connecting to the broker", "");
core::Property AbstractMQTTProcessor::Password("Password", "Password to use when connecting to the broker", "");
core::Property AbstractMQTTProcessor::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");
core::Property AbstractMQTTProcessor::SecurityCA("Security CA", "File or directory path to CA certificate(s) for verifying the broker's key", "");
core::Property AbstractMQTTProcessor::SecurityCert("Security Cert", "Path to client's public key (PEM) used for authentication", "");
core::Property AbstractMQTTProcessor::SecurityPrivateKey("Security Private Key", "Path to client's private key (PEM) used for authentication", "");
core::Property AbstractMQTTProcessor::SecurityPrivateKeyPassword("Security Pass Phrase", "Private key passphrase", "");

void AbstractMQTTProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*factory*/) {
  sslEnabled_ = false;

  std::string value;
  int64_t valInt;
  value = "";
  if (context->getProperty(BrokerURI.getName(), value) && !value.empty()) {
    uri_ = value;
    logger_->log_debug("AbstractMQTTProcessor: BrokerURI [%s]", uri_);
  }
  value = "";
  if (context->getProperty(ClientID.getName(), value) && !value.empty()) {
    clientID_ = value;
    logger_->log_debug("AbstractMQTTProcessor: ClientID [%s]", clientID_);
  }
  value = "";
  if (context->getProperty(Topic.getName(), value) && !value.empty()) {
    topic_ = value;
    logger_->log_debug("AbstractMQTTProcessor: Topic [%s]", topic_);
  }
  value = "";
  if (context->getProperty(Username.getName(), value) && !value.empty()) {
    username_ = value;
    logger_->log_debug("AbstractMQTTProcessor: UserName [%s]", username_);
  }
  value = "";
  if (context->getProperty(Password.getName(), value) && !value.empty()) {
    password_ = value;
    logger_->log_debug("AbstractMQTTProcessor: Password [%s]", password_);
  }

  if (auto keep_alive_interval = context->getProperty<core::TimePeriodValue>(KeepLiveInterval)) {
    keepAliveInterval_ = keep_alive_interval->getMilliseconds();
    logger_->log_debug("AbstractMQTTProcessor: KeepLiveInterval [%" PRId64 "] ms", int64_t{keepAliveInterval_.count()});
  }

  if (auto connection_timeout = context->getProperty<core::TimePeriodValue>(ConnectionTimeout)) {
    connectionTimeout_ = connection_timeout->getMilliseconds();
    logger_->log_debug("AbstractMQTTProcessor: ConnectionTimeout [%" PRId64 "] ms", int64_t{connectionTimeout_.count()});
  }

  value = "";
  if (context->getProperty(QoS.getName(), value) && !value.empty() && (value == MQTT_QOS_0 || value == MQTT_QOS_1 || MQTT_QOS_2) &&
      core::Property::StringToInt(value, valInt)) {
    qos_ = valInt;
    logger_->log_debug("AbstractMQTTProcessor: QOS [%" PRId64 "]", qos_);
  }
  value = "";

  if (context->getProperty(SecurityProtocol.getName(), value) && !value.empty()) {
    if (value == MQTT_SECURITY_PROTOCOL_SSL) {
      sslEnabled_ = true;
      value = "";
      if (context->getProperty(SecurityCA.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: trustStore [%s]", value);
        securityCA_ = value;
        sslOpts_.trustStore = securityCA_.c_str();
      }
      value = "";
      if (context->getProperty(SecurityCert.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: keyStore [%s]", value);
        securityCert_ = value;
        sslOpts_.keyStore = securityCert_.c_str();
      }
      value = "";
      if (context->getProperty(SecurityPrivateKey.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: privateKey [%s]", value);
        securityPrivateKey_ = value;
        sslOpts_.privateKey = securityPrivateKey_.c_str();
      }
      value = "";
      if (context->getProperty(SecurityPrivateKeyPassword.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: privateKeyPassword [%s]", value);
        securityPrivateKeyPassword_ = value;
        sslOpts_.privateKeyPassword = securityPrivateKeyPassword_.c_str();
      }
    }
  }

  if (!client_) {
    MQTTClient_create(&client_, uri_.c_str(), clientID_.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  }
  if (client_) {
    MQTTClient_setCallbacks(client_, this, connectionLost, msgReceived, msgDelivered);
    // TODO(amarkovics) we are running in asynchronous mode, we need to do something
    // TODO(amarkovics) MQTTClient_yield also needs to be called frequently
    // call reconnect to bootstrap
    this->reconnect();
  }
}

bool AbstractMQTTProcessor::reconnect() {
  if (!client_) {
    return false;
  }
  if (MQTTClient_isConnected(client_)) {
    return true;
  }
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  conn_opts.keepAliveInterval = std::chrono::duration_cast<std::chrono::seconds>(keepAliveInterval_).count();
  conn_opts.cleansession = getCleanSession();
  if (!username_.empty()) {
    conn_opts.username = username_.c_str();
    conn_opts.password = password_.c_str();
  }
  if (sslEnabled_) {
    conn_opts.ssl = &sslOpts_;
  }
  int ret = MQTTClient_connect(client_, &conn_opts);
  if (ret != MQTTCLIENT_SUCCESS) {
    logger_->log_error("Failed to connect to MQTT broker %s (%d)", uri_, ret);
    return false;
  }
  if (!startupClient()) {
    return false;
  }
  return true;
  // TODO(amarkovics) do something about return values at calls
}

  void AbstractMQTTProcessor::notifyStop() {
    if (client_ && MQTTClient_isConnected(client_)) {
      MQTTClient_disconnect(client_, std::chrono::milliseconds{connectionTimeout_}.count());
    }
    if (client_)
    MQTTClient_destroy(&client_);
  }

}  // namespace org::apache::nifi::minifi::processors
