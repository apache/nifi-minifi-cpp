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
#include "ConsumeMQTT.h"

#include <memory>
#include <string>
#include <set>
#include <cinttypes>
#include <vector>

#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void ConsumeMQTT::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ConsumeMQTT::enqueueReceivedMQTTMsg(SmartMessage message) {
  if (queue_.size_approx() >= max_queue_size_) {
    logger_->log_error("MQTT queue full");
    return;
  }

  logger_->log_debug("enqueuing MQTT message with length %d", message.contents->payloadlen);
  queue_.enqueue(std::move(message));
}

void ConsumeMQTT::readProperties(const std::shared_ptr<core::ProcessContext>& context) {
  if (auto value = context->getProperty(Topic)) {
    topic_ = std::move(*value);
  }
  logger_->log_debug("ConsumeMQTT: Topic [%s]", topic_);

  if (const auto value = context->getProperty<bool>(CleanSession)) {
    clean_session_ = *value;
  }
  logger_->log_debug("ConsumeMQTT: CleanSession [%d]", clean_session_);

  if (const auto value = context->getProperty<bool>(CleanStart)) {
    clean_start_ = *value;
  }
  logger_->log_debug("ConsumeMQTT: CleanStart [%d]", clean_start_);

  if (const auto session_expiry_interval = context->getProperty<core::TimePeriodValue>(SessionExpiryInterval)) {
    session_expiry_interval_ = std::chrono::duration_cast<std::chrono::seconds>(session_expiry_interval->getMilliseconds());
  }
  logger_->log_debug("ConsumeMQTT: SessionExpiryInterval [%" PRId64 "] s", int64_t{session_expiry_interval_.count()});

  if (const auto value = context->getProperty<uint64_t>(QueueBufferMaxMessage)) {
    max_queue_size_ = *value;
  }
  logger_->log_debug("ConsumeMQTT: Queue Max Message [%" PRIu64 "]", max_queue_size_);

  if (auto value = context->getProperty(AttributeFromContentType)) {
    attribute_from_content_type_ = std::move(*value);
  }
  logger_->log_debug("ConsumeMQTT: Attribute From Content Type [%s]", attribute_from_content_type_);

  if (const auto topic_alias_maximum = context->getProperty<uint32_t>(TopicAliasMaximum)) {
    topic_alias_maximum_ = gsl::narrow<uint16_t>(*topic_alias_maximum);
  }
  logger_->log_debug("ConsumeMQTT: Topic Alias Maximum [%" PRIu16 "]", topic_alias_maximum_);

  if (const auto receive_maximum = context->getProperty<uint32_t>(ReceiveMaximum)) {
    receive_maximum_ = gsl::narrow<uint16_t>(*receive_maximum);
  }
  logger_->log_debug("ConsumeMQTT: Receive Maximum [%" PRIu16 "]", receive_maximum_);
}

void ConsumeMQTT::onTriggerImpl(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& session) {
  std::queue<SmartMessage> msg_queue = getReceivedMqttMessages();
  while (!msg_queue.empty()) {
    const auto& message = msg_queue.front();
    std::shared_ptr<core::FlowFile> flow_file = session->create();
    WriteCallback write_callback(message, logger_);
    try {
      session->write(flow_file, std::ref(write_callback));
    } catch (const Exception& ex) {
      logger_->log_error("Error when processing message queue: %s", ex.what());
    }
    if (!write_callback.getSuccessStatus()) {
      logger_->log_error("ConsumeMQTT fail for the flow with UUID %s", flow_file->getUUIDStr());
      session->remove(flow_file);
    } else {
      putUserPropertiesAsAttributes(message, flow_file, session);
      session->putAttribute(flow_file, std::string(BrokerOutputAttribute.name), uri_);
      session->putAttribute(flow_file, std::string(TopicOutputAttribute.name), message.topic);
      fillAttributeFromContentType(message, flow_file, session);
      logger_->log_debug("ConsumeMQTT processing success for the flow with UUID %s topic %s", flow_file->getUUIDStr(), message.topic);
      session->transfer(flow_file, Success);
    }
    msg_queue.pop();
  }
}

std::queue<ConsumeMQTT::SmartMessage> ConsumeMQTT::getReceivedMqttMessages() {
  std::queue<SmartMessage> msg_queue;
  SmartMessage message;
  while (queue_.try_dequeue(message)) {
    msg_queue.push(std::move(message));
  }
  return msg_queue;
}

int64_t ConsumeMQTT::WriteCallback::operator() (const std::shared_ptr<io::OutputStream>& stream) {
  if (message_.contents->payloadlen < 0) {
    success_status_ = false;
    logger_->log_error("Payload length of message is negative, value is [%d]", message_.contents->payloadlen);
    return -1;
  }

  const auto len = stream->write(reinterpret_cast<uint8_t*>(message_.contents->payload), gsl::narrow<size_t>(message_.contents->payloadlen));
  if (io::isError(len)) {
    success_status_ = false;
    logger_->log_error("Stream writing error when processing message");
    return -1;
  }

  return len;
}

void ConsumeMQTT::putUserPropertiesAsAttributes(const SmartMessage& message, const std::shared_ptr<core::FlowFile>& flow_file, const std::shared_ptr<core::ProcessSession>& session) const {
  if (mqtt_version_.value() != mqtt::MqttVersions::V_5_0) {
    return;
  }

  const auto property_count = MQTTProperties_propertyCount(&message.contents->properties, MQTTPROPERTY_CODE_USER_PROPERTY);
  for (int i=0; i < property_count; ++i) {
    MQTTProperty* property = MQTTProperties_getPropertyAt(&message.contents->properties, MQTTPROPERTY_CODE_USER_PROPERTY, i);
    std::string key(property->value.data.data, property->value.data.len);
    std::string value(property->value.value.data, property->value.value.len);
    session->putAttribute(flow_file, key, value);
  }
}

void ConsumeMQTT::fillAttributeFromContentType(const SmartMessage& message, const std::shared_ptr<core::FlowFile>& flow_file, const std::shared_ptr<core::ProcessSession>& session) const {
  if (mqtt_version_.value() != mqtt::MqttVersions::V_5_0 || attribute_from_content_type_.empty()) {
    return;
  }

  MQTTProperty* property = MQTTProperties_getProperty(&message.contents->properties, MQTTPROPERTY_CODE_CONTENT_TYPE);
  if (property == nullptr) {
    return;
  }

  std::string content_type(property->value.data.data, property->value.data.len);
  session->putAttribute(flow_file, attribute_from_content_type_, content_type);
}

void ConsumeMQTT::startupClient() {
  MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
  response_options.context = this;

  if (mqtt_version_.value() == mqtt::MqttVersions::V_5_0) {
    response_options.onSuccess5 = subscriptionSuccess5;
    response_options.onFailure5 = subscriptionFailure5;
  } else {
    response_options.onSuccess = subscriptionSuccess;
    response_options.onFailure = subscriptionFailure;
  }

  const int ret = MQTTAsync_subscribe(client_, topic_.c_str(), gsl::narrow<int>(qos_.value()), &response_options);
  if (ret != MQTTASYNC_SUCCESS) {
    logger_->log_error("Failed to subscribe to MQTT topic %s (%d)", topic_, ret);
    return;
  }
  logger_->log_debug("Successfully subscribed to MQTT topic: %s", topic_);
}

void ConsumeMQTT::onMessageReceived(SmartMessage smart_message) {
  if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
    resolveTopicFromAlias(smart_message);
  }

  if (smart_message.topic.empty()) {
    logger_->log_error("Received message without topic");
    return;
  }

  enqueueReceivedMQTTMsg(std::move(smart_message));
}

void ConsumeMQTT::resolveTopicFromAlias(SmartMessage& smart_message) {
  auto raw_alias = MQTTProperties_getNumericValue(&smart_message.contents->properties, MQTTPROPERTY_CODE_TOPIC_ALIAS);

  std::optional<uint16_t> alias;
  if (raw_alias != PAHO_MQTT_C_FAILURE_CODE) {
    alias = gsl::narrow<uint16_t>(raw_alias);
  }

  auto& topic = smart_message.topic;

  if (alias.has_value()) {
    if (*alias > topic_alias_maximum_) {
      logger_->log_error("Broker does not respect client's Topic Alias Maximum, sent a greater value: %" PRIu16 " > %" PRIu16, *alias, topic_alias_maximum_);
      return;
    }

    // if topic is empty, this is just a usage of a previously stored alias (look it up), otherwise a new one (store it)
    if (topic.empty()) {
      const auto iter = alias_to_topic_.find(*alias);
      if (iter == alias_to_topic_.end()) {
        logger_->log_error("Broker sent an alias that was not known to client before: %" PRIu16, *alias);
      } else {
        topic = iter->second;
      }
    } else {
      alias_to_topic_[*alias] = topic;
    }
  } else if (topic.empty()) {
    logger_->log_error("Received message without topic and alias");
  }
}

void ConsumeMQTT::checkProperties() {
  if (mqtt_version_ == mqtt::MqttVersions::V_3_1_0 || mqtt_version_ == mqtt::MqttVersions::V_3_1_1 || mqtt_version_ == mqtt::MqttVersions::V_3X_AUTO) {
    if (isPropertyExplicitlySet(CleanStart)) {
      logger_->log_warn("MQTT 3.x specification does not support Clean Start. Property is not used.");
    }
    if (isPropertyExplicitlySet(SessionExpiryInterval)) {
      logger_->log_warn("MQTT 3.x specification does not support Session Expiry Intervals. Property is not used.");
    }
    if (isPropertyExplicitlySet(AttributeFromContentType)) {
      logger_->log_warn("MQTT 3.x specification does not support Content Types and thus attributes cannot be created from them. Property is not used.");
    }
    if (isPropertyExplicitlySet(TopicAliasMaximum)) {
      logger_->log_warn("MQTT 3.x specification does not support Topic Alias Maximum. Property is not used.");
    }
    if (isPropertyExplicitlySet(ReceiveMaximum)) {
      logger_->log_warn("MQTT 3.x specification does not support Receive Maximum. Property is not used.");
    }
  }

  if (mqtt_version_.value() == mqtt::MqttVersions::V_5_0 && isPropertyExplicitlySet(CleanSession)) {
    logger_->log_warn("MQTT 5.0 specification does not support Clean Session. Property is not used.");
  }

  if (clientID_.empty()) {
    if (mqtt_version_.value() == mqtt::MqttVersions::V_5_0) {
      if (session_expiry_interval_ > std::chrono::seconds(0)) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Processor must have a Client ID for durable (Session Expiry Interval > 0) sessions");
      }
    } else if (!clean_session_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Processor must have a Client ID for durable (non-clean) sessions");
    }
  }

  if (qos_ == mqtt::MqttQoS::LEVEL_0) {
    if (mqtt_version_.value() == mqtt::MqttVersions::V_5_0) {
      if (session_expiry_interval_ > std::chrono::seconds(0)) {
        logger_->log_warn("Messages are not preserved during client disconnection "
                          "by the broker when QoS is less than 1 for durable (Session Expiry Interval > 0) sessions. Only subscriptions are preserved.");
      }
    } else if (!clean_session_) {
      logger_->log_warn("Messages are not preserved during client disconnection "
                        "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved.");
    }
  }
}

void ConsumeMQTT::checkBrokerLimitsImpl() {
  auto hasWildcards = [] (std::string_view topic) {
    return std::any_of(topic.begin(), topic.end(), [] (const char ch) {return ch == '+' || ch == '#';});
  };

  if (wildcard_subscription_available_ == false && hasWildcards(topic_)) {
    std::ostringstream os;
    os << "Broker does not support wildcards but topic \"" << topic_ <<"\" has them";
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, os.str());
  }

  if (maximum_session_expiry_interval_.has_value() && session_expiry_interval_ > maximum_session_expiry_interval_) {
    std::ostringstream os;
    os << "Set Session Expiry Interval (" << session_expiry_interval_.count() <<" s) is longer than the maximum supported by the broker (" << maximum_session_expiry_interval_->count() << " s).";
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, os.str());
  }

  if (utils::StringUtils::startsWith(topic_, "$share/")) {
    if (mqtt_version_.value() == mqtt::MqttVersions::V_5_0) {
      // shared topic are supported on MQTT 5, unless explicitly denied by broker
      if (shared_subscription_available_ == false) {
        std::ostringstream os;
        os << "Shared topic feature with topic \"" << topic_ << "\" is not supported by broker";
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, os.str());
      }
    } else {
      logger_->log_warn("Shared topic feature with topic \"%s\" might not be supported by broker on MQTT 3.x");
    }
  }
}

void ConsumeMQTT::setProcessorSpecificMqtt5ConnectOptions(MQTTProperties& connect_props) const {
  if (topic_alias_maximum_ > 0) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_TOPIC_ALIAS_MAXIMUM;
    property.value.integer2 = topic_alias_maximum_;
    MQTTProperties_add(&connect_props, &property);
  }

  if (receive_maximum_ < MQTT_MAX_RECEIVE_MAXIMUM) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_RECEIVE_MAXIMUM;
    property.value.integer2 = receive_maximum_;
    MQTTProperties_add(&connect_props, &property);
  }
}

void ConsumeMQTT::subscriptionSuccess(void* context, MQTTAsync_successData* /*response*/) {
  auto* processor = reinterpret_cast<ConsumeMQTT*>(context);
  processor->onSubscriptionSuccess();
}

void ConsumeMQTT::subscriptionSuccess5(void* context, MQTTAsync_successData5* /*response*/) {
  auto* processor = reinterpret_cast<ConsumeMQTT*>(context);
  processor->onSubscriptionSuccess();
}

void ConsumeMQTT::subscriptionFailure(void* context, MQTTAsync_failureData* response) {
  auto* processor = reinterpret_cast<ConsumeMQTT*>(context);
  processor->onSubscriptionFailure(response);
}

void ConsumeMQTT::subscriptionFailure5(void* context, MQTTAsync_failureData5* response) {
  auto* processor = reinterpret_cast<ConsumeMQTT*>(context);
  processor->onSubscriptionFailure5(response);
}

void ConsumeMQTT::onSubscriptionSuccess() {
  logger_->log_info("Successfully subscribed to MQTT topic %s on broker %s", topic_, uri_);
}

void ConsumeMQTT::onSubscriptionFailure(MQTTAsync_failureData* response) {
  logger_->log_error("Subscription failed on topic %s to MQTT broker %s (%d)", topic_, uri_, response->code);
  if (response->message != nullptr) {
    logger_->log_error("Detailed reason for subscription failure: %s", response->message);
  }
}

void ConsumeMQTT::onSubscriptionFailure5(MQTTAsync_failureData5* response) {
  logger_->log_error("Subscription failed on topic %s to MQTT broker %s (%d)", topic_, uri_, response->code);
  if (response->message != nullptr) {
    logger_->log_error("Detailed reason for subscription failure: %s", response->message);
  }
  logger_->log_error("Reason code for subscription failure: %d: %s", response->reasonCode, MQTTReasonCode_toString(response->reasonCode));
}

REGISTER_RESOURCE(ConsumeMQTT, Processor);

}  // namespace org::apache::nifi::minifi::processors
