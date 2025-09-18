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


#include <cinttypes>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/ValueParser.h"
#include "utils/ProcessorConfigUtils.h"

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

  logger_->log_debug("enqueuing MQTT message with length {}", message.contents->payloadlen);
  queue_.enqueue(std::move(message));
}

void ConsumeMQTT::readProperties(core::ProcessContext& context) {
  topic_ = utils::parseProperty(context, Topic);
  clean_session_ = utils::parseBoolProperty(context, CleanSession);
  clean_start_ = utils::parseBoolProperty(context, CleanStart);
  session_expiry_interval_ = std::chrono::duration_cast<std::chrono::seconds>(utils::parseDurationProperty(context, SessionExpiryInterval));
  max_queue_size_ = utils::parseU64Property(context, QueueBufferMaxMessage);
  attribute_from_content_type_ = context.getProperty(AttributeFromContentType).value_or("");
  topic_alias_maximum_ = gsl::narrow<uint16_t>(utils::parseU64Property(context, QueueBufferMaxMessage));
  receive_maximum_ = gsl::narrow<uint16_t>(utils::parseU64Property(context, ReceiveMaximum));
}

void ConsumeMQTT::onTriggerImpl(core::ProcessContext&, core::ProcessSession& session) {
  std::queue<SmartMessage> msg_queue = getReceivedMqttMessages();
  while (!msg_queue.empty()) {
    const auto& message = msg_queue.front();
    std::shared_ptr<core::FlowFile> flow_file = session.create();
    WriteCallback write_callback(message, logger_);
    try {
      session.write(flow_file, std::ref(write_callback));
    } catch (const Exception& ex) {
      logger_->log_error("Error when processing message queue: {}", ex.what());
    }
    if (!write_callback.getSuccessStatus()) {
      logger_->log_error("ConsumeMQTT fail for the flow with UUID {}", flow_file->getUUIDStr());
      session.remove(flow_file);
    } else {
      putUserPropertiesAsAttributes(message, flow_file, session);
      session.putAttribute(*flow_file, BrokerOutputAttribute.name, uri_);
      session.putAttribute(*flow_file, TopicOutputAttribute.name, message.topic);
      fillAttributeFromContentType(message, flow_file, session);
      logger_->log_debug("ConsumeMQTT processing success for the flow with UUID {} topic {}", flow_file->getUUIDStr(), message.topic);
      session.transfer(flow_file, Success);
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
    logger_->log_error("Payload length of message is negative, value is [{}]", message_.contents->payloadlen);
    return -1;
  }

  const auto len = stream->write(reinterpret_cast<uint8_t*>(message_.contents->payload), gsl::narrow<size_t>(message_.contents->payloadlen));
  if (io::isError(len)) {
    success_status_ = false;
    logger_->log_error("Stream writing error when processing message");
    return -1;
  }

  return gsl::narrow<int64_t>(len);
}

void ConsumeMQTT::putUserPropertiesAsAttributes(const SmartMessage& message, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) const {
  if (mqtt_version_ != mqtt::MqttVersions::V_5_0) {
    return;
  }

  const auto property_count = MQTTProperties_propertyCount(&message.contents->properties, MQTTPROPERTY_CODE_USER_PROPERTY);
  for (int i=0; i < property_count; ++i) {
    MQTTProperty* property = MQTTProperties_getPropertyAt(&message.contents->properties, MQTTPROPERTY_CODE_USER_PROPERTY, i);
    std::string key(property->value.data.data, property->value.data.len);  // NOLINT(cppcoreguidelines-pro-type-union-access)
    std::string value(property->value.value.data, property->value.value.len);  // NOLINT(cppcoreguidelines-pro-type-union-access)
    session.putAttribute(*flow_file, key, value);
  }
}

void ConsumeMQTT::fillAttributeFromContentType(const SmartMessage& message, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) const {
  if (mqtt_version_ != mqtt::MqttVersions::V_5_0 || attribute_from_content_type_.empty()) {
    return;
  }

  MQTTProperty* property = MQTTProperties_getProperty(&message.contents->properties, MQTTPROPERTY_CODE_CONTENT_TYPE);
  if (property == nullptr) {
    return;
  }

  std::string content_type(property->value.data.data, property->value.data.len);  // NOLINT(cppcoreguidelines-pro-type-union-access)
  session.putAttribute(*flow_file, attribute_from_content_type_, content_type);
}

void ConsumeMQTT::startupClient() {
  MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
  response_options.context = this;

  if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
    response_options.onSuccess5 = subscriptionSuccess5;
    response_options.onFailure5 = subscriptionFailure5;
  } else {
    response_options.onSuccess = subscriptionSuccess;
    response_options.onFailure = subscriptionFailure;
  }

  const int ret = MQTTAsync_subscribe(client_, topic_.c_str(), gsl::narrow<int>(qos_), &response_options);
  if (ret != MQTTASYNC_SUCCESS) {
    logger_->log_error("Failed to subscribe to MQTT topic {} ({})", topic_, ret);
    return;
  }
  logger_->log_debug("Successfully subscribed to MQTT topic: {}", topic_);
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
      logger_->log_error("Broker does not respect client's Topic Alias Maximum, sent a greater value: {} > {}", *alias, topic_alias_maximum_);
      return;
    }

    // if topic is empty, this is just a usage of a previously stored alias (look it up), otherwise a new one (store it)
    if (topic.empty()) {
      const auto iter = alias_to_topic_.find(*alias);
      if (iter == alias_to_topic_.end()) {
        logger_->log_error("Broker sent an alias that was not known to client before: {}", *alias);
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


void ConsumeMQTT::checkProperties(core::ProcessContext& context) {
  auto is_property_explicitly_set = [&context](const std::string_view property_name) -> bool {
    const auto property_values = context.getAllPropertyValues(property_name) | utils::orThrow("It should only be called on valid property");
    return !property_values.empty();
  };
  if (mqtt_version_ == mqtt::MqttVersions::V_3_1_0 || mqtt_version_ == mqtt::MqttVersions::V_3_1_1 || mqtt_version_ == mqtt::MqttVersions::V_3X_AUTO) {
    if (is_property_explicitly_set(CleanStart.name)) {
      logger_->log_warn("MQTT 3.x specification does not support Clean Start. Property is not used.");
    }
    if (is_property_explicitly_set(SessionExpiryInterval.name)) {
      logger_->log_warn("MQTT 3.x specification does not support Session Expiry Intervals. Property is not used.");
    }
    if (is_property_explicitly_set(AttributeFromContentType.name)) {
      logger_->log_warn("MQTT 3.x specification does not support Content Types and thus attributes cannot be created from them. Property is not used.");
    }
    if (is_property_explicitly_set(TopicAliasMaximum.name)) {
      logger_->log_warn("MQTT 3.x specification does not support Topic Alias Maximum. Property is not used.");
    }
    if (is_property_explicitly_set(ReceiveMaximum.name)) {
      logger_->log_warn("MQTT 3.x specification does not support Receive Maximum. Property is not used.");
    }
  }

  if (mqtt_version_ == mqtt::MqttVersions::V_5_0 && is_property_explicitly_set(CleanSession.name)) {
    logger_->log_warn("MQTT 5.0 specification does not support Clean Session. Property is not used.");
  }

  if (clientID_.empty()) {
    if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
      if (session_expiry_interval_ > std::chrono::seconds(0)) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Processor must have a Client ID for durable (Session Expiry Interval > 0) sessions");
      }
    } else if (!clean_session_) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Processor must have a Client ID for durable (non-clean) sessions");
    }
  }

  if (qos_ == mqtt::MqttQoS::LEVEL_0) {
    if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
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

  if (utils::string::startsWith(topic_, "$share/")) {
    if (mqtt_version_ == mqtt::MqttVersions::V_5_0) {
      // shared topic are supported on MQTT 5, unless explicitly denied by broker
      if (shared_subscription_available_ == false) {
        std::ostringstream os;
        os << "Shared topic feature with topic \"" << topic_ << "\" is not supported by broker";
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, os.str());
      }
    } else {
      logger_->log_warn("Shared topic feature with topic \"{}\" might not be supported by broker on MQTT 3.x", topic_);
    }
  }
}

void ConsumeMQTT::setProcessorSpecificMqtt5ConnectOptions(MQTTProperties& connect_props) const {
  if (topic_alias_maximum_ > 0) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_TOPIC_ALIAS_MAXIMUM;
    property.value.integer2 = topic_alias_maximum_;  // NOLINT(cppcoreguidelines-pro-type-union-access)
    MQTTProperties_add(&connect_props, &property);
  }

  if (receive_maximum_ < MQTT_MAX_RECEIVE_MAXIMUM) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_RECEIVE_MAXIMUM;
    property.value.integer2 = receive_maximum_;  // NOLINT(cppcoreguidelines-pro-type-union-access)
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
  logger_->log_info("Successfully subscribed to MQTT topic {} on broker {}", topic_, uri_);
}

void ConsumeMQTT::onSubscriptionFailure(MQTTAsync_failureData* response) {
  logger_->log_error("Subscription failed on topic {} to MQTT broker {} ({})", topic_, uri_, response->code);
  if (response->message != nullptr) {
    logger_->log_error("Detailed reason for subscription failure: {}", response->message);
  }
}

void ConsumeMQTT::onSubscriptionFailure5(MQTTAsync_failureData5* response) {
  logger_->log_error("Subscription failed on topic {} to MQTT broker {} ({})", topic_, uri_, response->code);
  if (response->message != nullptr) {
    logger_->log_error("Detailed reason for subscription failure: {}", response->message);
  }
  logger_->log_error("Reason code for subscription failure: {}: {}", magic_enum::enum_underlying(response->reasonCode), MQTTReasonCode_toString(response->reasonCode));
}

REGISTER_RESOURCE(ConsumeMQTT, Processor);

}  // namespace org::apache::nifi::minifi::processors
