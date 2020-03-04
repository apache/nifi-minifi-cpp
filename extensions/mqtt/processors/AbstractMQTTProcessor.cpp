/**
 * @file AbstractMQTTProcessor.cpp
 * AbstractMQTTProcessor class implementation
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
#include "AbstractMQTTProcessor.h"
#include <stdio.h>
#include <memory>
#include <string>
#include <cinttypes>

#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property AbstractMQTTProcessor::BrokerURL("Broker URI", "The URI to use to connect to the MQTT broker", "");
core::Property AbstractMQTTProcessor::CleanSession("Session state", "Whether to start afresh or resume previous flows. See the allowable value descriptions for more details", "true");
core::Property AbstractMQTTProcessor::ClientID("Client ID", "MQTT client ID to use", "");
core::Property AbstractMQTTProcessor::UserName("Username", "Username to use when connecting to the broker", "");
core::Property AbstractMQTTProcessor::PassWord("Password", "Password to use when connecting to the broker", "");
core::Property AbstractMQTTProcessor::KeepLiveInterval("Keep Alive Interval", "Defines the maximum time interval between messages sent or received", "60 sec");
core::Property AbstractMQTTProcessor::ConnectionTimeOut("Connection Timeout", "Maximum time interval the client will wait for the network connection to the MQTT server", "30 sec");
core::Property AbstractMQTTProcessor::QOS("Quality of Service", "The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2'", "MQTT_QOS_0");
core::Property AbstractMQTTProcessor::Topic("Topic", "The topic to publish the message to", "");
core::Property AbstractMQTTProcessor::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");
core::Property AbstractMQTTProcessor::SecurityCA("Security CA", "File or directory path to CA certificate(s) for verifying the broker's key", "");
core::Property AbstractMQTTProcessor::SecurityCert("Security Cert", "Path to client's public key (PEM) used for authentication", "");
core::Property AbstractMQTTProcessor::SecurityPrivateKey("Security Private Key", "Path to client's private key (PEM) used for authentication", "");
core::Property AbstractMQTTProcessor::SecurityPrivateKeyPassWord("Security Pass Phrase", "Private key passphrase", "");

const std::set<core::Property> AbstractMQTTProcessor::getSupportedProperties() {
  return {BrokerURL, CleanSession, ClientID, UserName, PassWord, KeepLiveInterval, ConnectionTimeOut, QOS, Topic};
}

void AbstractMQTTProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
  sslEnabled_ = false;
  sslopts_ = MQTTClient_SSLOptions_initializer;

  std::string value;
  int64_t valInt;
  value = "";
  if (context->getProperty(BrokerURL.getName(), value) && !value.empty()) {
    uri_ = value;
    logger_->log_debug("AbstractMQTTProcessor: BrokerURL [%s]", uri_);
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
  if (context->getProperty(UserName.getName(), value) && !value.empty()) {
    userName_ = value;
    logger_->log_debug("AbstractMQTTProcessor: UserName [%s]", userName_);
  }
  value = "";
  if (context->getProperty(PassWord.getName(), value) && !value.empty()) {
    passWord_ = value;
    logger_->log_debug("AbstractMQTTProcessor: PassWord [%s]", passWord_);
  }
  value = "";
  if (context->getProperty(CleanSession.getName(), value) && !value.empty() &&
      org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, cleanSession_)) {
    logger_->log_debug("AbstractMQTTProcessor: CleanSession [%d]", cleanSession_);
  }
  value = "";
  if (context->getProperty(KeepLiveInterval.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      keepAliveInterval_ = valInt/1000;
      logger_->log_debug("AbstractMQTTProcessor: KeepLiveInterval [%" PRId64 "]", keepAliveInterval_);
    }
  }
  value = "";
  if (context->getProperty(ConnectionTimeOut.getName(), value) && !value.empty()) {
    core::TimeUnit unit;
    if (core::Property::StringToTime(value, valInt, unit) && core::Property::ConvertTimeUnitToMS(valInt, unit, valInt)) {
      connectionTimeOut_ = valInt/1000;
      logger_->log_debug("AbstractMQTTProcessor: ConnectionTimeOut [%" PRId64 "]", connectionTimeOut_);
    }
  }
  value = "";
  if (context->getProperty(QOS.getName(), value) && !value.empty() && (value == MQTT_QOS_0 || value == MQTT_QOS_1 || MQTT_QOS_2) &&
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
        sslopts_.trustStore = securityCA_.c_str();
      }
      value = "";
      if (context->getProperty(SecurityCert.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: keyStore [%s]", value);
        securityCert_ = value;
        sslopts_.keyStore = securityCert_.c_str();
      }
      value = "";
      if (context->getProperty(SecurityPrivateKey.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: privateKey [%s]", value);
        securityPrivateKey_ = value;
        sslopts_.privateKey = securityPrivateKey_.c_str();
      }
      value = "";
      if (context->getProperty(SecurityPrivateKeyPassWord.getName(), value) && !value.empty()) {
        logger_->log_debug("AbstractMQTTProcessor: privateKeyPassword [%s]", value);
        securityPrivateKeyPassWord_ = value;
        sslopts_.privateKeyPassword = securityPrivateKeyPassWord_.c_str();
      }
    }
  }
  if (!client_) {
    MQTTClient_create(&client_, uri_.c_str(), clientID_.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
  }
  if (client_) {
    MQTTClient_setCallbacks(client_, (void *) this, connectionLost, msgReceived, msgDelivered);
    // call reconnect to bootstrap
    this->reconnect();
  }
}

bool AbstractMQTTProcessor::reconnect() {
  if (!client_)
    return false;
  if (MQTTClient_isConnected(client_))
    return true;
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  conn_opts.keepAliveInterval = keepAliveInterval_;
  conn_opts.cleansession = cleanSession_;
  if (!userName_.empty()) {
    conn_opts.username = userName_.c_str();
    conn_opts.password = passWord_.c_str();
  }
  if (sslEnabled_) {
    conn_opts.ssl = &sslopts_;
  }
  int ret = MQTTClient_connect(client_, &conn_opts);
  if (ret != MQTTCLIENT_SUCCESS) {
    logger_->log_error("Failed to connect to MQTT broker %s (%d)", uri_, ret);
    return false;
  }
  if (isSubscriber_) {
    ret = MQTTClient_subscribe(client_, topic_.c_str(), qos_);
    if(ret != MQTTCLIENT_SUCCESS) {
      logger_->log_error("Failed to subscribe to MQTT topic %s (%d)", topic_, ret);
      return false;
    }
    logger_->log_debug("Successfully subscribed to MQTT topic: %s", topic_);
  }
  return true;
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
