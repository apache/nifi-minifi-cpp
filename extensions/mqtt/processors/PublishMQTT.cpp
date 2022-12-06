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
#include "PublishMQTT.h"

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <optional>
#include <vector>

#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

using SendFinishedTask = std::packaged_task<bool(bool, std::optional<int>, std::optional<MQTTReasonCodes>)>;

void PublishMQTT::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void PublishMQTT::readProperties(const std::shared_ptr<core::ProcessContext>& context) {
  if (const auto retain_opt = context->getProperty<bool>(Retain)) {
    retain_ = *retain_opt;
  }
  logger_->log_debug("PublishMQTT: Retain [%d]", retain_);

  if (const auto message_expiry_interval = context->getProperty<core::TimePeriodValue>(MessageExpiryInterval)) {
    message_expiry_interval_ = std::chrono::duration_cast<std::chrono::seconds>(message_expiry_interval->getMilliseconds());
    logger_->log_debug("PublishMQTT: MessageExpiryInterval [%" PRId64 "] s", int64_t{message_expiry_interval_->count()});
  }

  in_flight_message_counter_.setMqttVersion(mqtt_version_);
  in_flight_message_counter_.setQoS(qos_);
}

void PublishMQTT::onTriggerImpl(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  std::shared_ptr<core::FlowFile> flow_file = session->get();

  if (!flow_file) {
    yield();
    return;
  }

  // broker's Receive Maximum can change after reconnect
  in_flight_message_counter_.setMax(broker_receive_maximum_.value_or(MQTT_MAX_RECEIVE_MAXIMUM));

  const auto topic = getTopic(context, flow_file);
  try {
    const auto result = session->readBuffer(flow_file);
    if (result.status < 0 || !sendMessage(result.buffer, topic, getContentType(context, flow_file), flow_file)) {
      logger_->log_error("Failed to send flow file [%s] to MQTT topic '%s' on broker %s", flow_file->getUUIDStr(), topic, uri_);
      session->transfer(flow_file, Failure);
      return;
    }
    logger_->log_debug("Sent flow file [%s] with length %" PRId64 " to MQTT topic '%s' on broker %s", flow_file->getUUIDStr(), result.status, topic, uri_);
    session->transfer(flow_file, Success);
  } catch (const Exception& ex) {
    logger_->log_error("Failed to send flow file [%s] to MQTT topic '%s' on broker %s, exception string: '%s'", flow_file->getUUIDStr(), topic, uri_, ex.what());
    session->transfer(flow_file, Failure);
  }
}

bool PublishMQTT::sendMessage(const std::vector<std::byte>& buffer, const std::string& topic, const std::string& content_type, const std::shared_ptr<core::FlowFile>& flow_file) {
  static const unsigned max_packet_size = 268'435'455;
  if (buffer.size() > max_packet_size) {
    logger_->log_error("Sending message failed because MQTT limit maximum packet size [%u] is exceeded by FlowFile of [%zu]", std::to_string(max_packet_size), buffer.size());
    return false;
  }

  if (maximum_packet_size_.has_value() && buffer.size() > *(maximum_packet_size_)) {
    logger_->log_error("Sending message failed because broker-requested maximum packet size [%" PRIu32 "] is exceeded by FlowFile of [%zu]",
                                   *maximum_packet_size_, buffer.size());
    return false;
  }

  MQTTAsync_message message_to_publish = MQTTAsync_message_initializer;
  message_to_publish.payload = const_cast<std::byte*>(buffer.data());
  message_to_publish.payloadlen = buffer.size();
  message_to_publish.qos = qos_.value();
  message_to_publish.retained = retain_;

  setMqtt5Properties(message_to_publish, content_type, flow_file);

  MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
  if (mqtt_version_ == MqttVersions::V_5_0) {
    response_options.onSuccess5 = sendSuccess5;
    response_options.onFailure5 = sendFailure5;
  } else {
    response_options.onSuccess = sendSuccess;
    response_options.onFailure = sendFailure;
  }

  // save context for callback
  SendFinishedTask send_finished_task(
          [this] (const bool success, const std::optional<int> response_code, const std::optional<MQTTReasonCodes> reason_code) {
            return notify(success, response_code, reason_code);
          });
  response_options.context = &send_finished_task;

  in_flight_message_counter_.increase();

  const int error_code = MQTTAsync_sendMessage(client_, topic.c_str(), &message_to_publish, &response_options);
  if (error_code != MQTTASYNC_SUCCESS) {
    logger_->log_error("MQTTAsync_sendMessage failed on topic '%s', MQTT broker %s with error code [%d]", topic, uri_, error_code);
    // early fail, sending attempt did not succeed, no need to wait for callback
    in_flight_message_counter_.decrease();
    return false;
  }

  return send_finished_task.get_future().get();
}

void PublishMQTT::checkProperties() {
  if ((mqtt_version_ == MqttVersions::V_3_1_0 || mqtt_version_ == MqttVersions::V_3_1_1 || mqtt_version_ == MqttVersions::V_3X_AUTO)) {
    if (isPropertyExplicitlySet(MessageExpiryInterval)) {
      logger_->log_warn("MQTT 3.x specification does not support Message Expiry Intervals. Property is not used.");
    }
    if (isPropertyExplicitlySet(ContentType)) {
      logger_->log_warn("MQTT 3.x specification does not support Content Types. Property is not used.");
    }
  }
}

void PublishMQTT::checkBrokerLimitsImpl() {
  if (retain_available_ == false && retain_) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Retain was set but broker does not support it");
  }
}

void PublishMQTT::sendSuccess(void* context, MQTTAsync_successData* /*response*/) {
  auto send_finished_task = reinterpret_cast<SendFinishedTask*>(context);
  (*send_finished_task)(true, std::nullopt, std::nullopt);
}

void PublishMQTT::sendSuccess5(void* context, MQTTAsync_successData5* response) {
  auto send_finished_task = reinterpret_cast<SendFinishedTask*>(context);
  (*send_finished_task)(true, std::nullopt, response->reasonCode);
}

void PublishMQTT::sendFailure(void* context, MQTTAsync_failureData* response) {
  auto send_finished_task = reinterpret_cast<SendFinishedTask*>(context);
  (*send_finished_task)(false, response->code, std::nullopt);
}

void PublishMQTT::sendFailure5(void* context, MQTTAsync_failureData5* response) {
  auto send_finished_task = reinterpret_cast<SendFinishedTask*>(context);
  (*send_finished_task)(false, response->code, response->reasonCode);
}

bool PublishMQTT::notify(const bool success, const std::optional<int> response_code, const std::optional<MQTTReasonCodes> reason_code) {
  in_flight_message_counter_.decrease();

  if (success) {
    logger_->log_debug("Successfully sent message to MQTT broker %s", uri_);
    if (reason_code.has_value()) {
      logger_->log_error("Additional reason code for sending success: %d: %s", *reason_code, MQTTReasonCode_toString(*reason_code));
    }
  } else {
    logger_->log_error("Sending message failed to MQTT broker %s with response code %d", uri_, *response_code);
    if (reason_code.has_value()) {
      logger_->log_error("Reason code for sending failure: %d: %s", *reason_code, MQTTReasonCode_toString(*reason_code));
    }
  }

  return success;
}

std::string PublishMQTT::getTopic(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const {
  if (auto value = context->getProperty(Topic, flow_file)) {
    logger_->log_debug("PublishMQTT: Topic resolved as \"%s\"", *value);
    return *value;
  }
  throw minifi::Exception(ExceptionType::GENERAL_EXCEPTION, "Could not resolve required property Topic");
}

std::string PublishMQTT::getContentType(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::FlowFile>& flow_file) const {
  if (auto value = context->getProperty(ContentType, flow_file)) {
    logger_->log_debug("PublishMQTT: Content Type resolved as \"%s\"", *value);
    return *value;
  }
  return "";
}

void PublishMQTT::setMqtt5Properties(MQTTAsync_message& message, const std::string& content_type, const std::shared_ptr<core::FlowFile>& flow_file) const {
  if (mqtt_version_ != MqttVersions::V_5_0) {
    return;
  }

  if (message_expiry_interval_.has_value()) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_MESSAGE_EXPIRY_INTERVAL;
    property.value.integer4 = message_expiry_interval_->count();
    MQTTProperties_add(&message.properties, &property);
  }

  if (!content_type.empty()) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_CONTENT_TYPE;
    property.value.data.len = content_type.length();
    property.value.data.data = const_cast<char*>(content_type.data());
    MQTTProperties_add(&message.properties, &property);
  }

  addAttributesAsUserProperties(message, flow_file);
}

void PublishMQTT::addAttributesAsUserProperties(MQTTAsync_message& message, const std::shared_ptr<core::FlowFile>& flow_file) {
  for (const auto& [key, value] : *flow_file->getAttributesPtr()) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_USER_PROPERTY;

    // key
    property.value.data.len = key.length();
    property.value.data.data = const_cast<char*>(key.data());

    // value
    property.value.value.len = value.length();
    property.value.value.data = const_cast<char*>(value.data());

    MQTTProperties_add(&message.properties, &property);
  }
}

void PublishMQTT::InFlightMessageCounter::increase() {
  if (mqtt_version_ != MqttVersions::V_5_0 || qos_ == MqttQoS::LEVEL_0) {
    return;
  }

  std::unique_lock lock{mutex_};
  cv_.wait(lock, [this] {return counter_ < limit_;});
  ++counter_;
}

void PublishMQTT::InFlightMessageCounter::decrease() {
  if (mqtt_version_ != MqttVersions::V_5_0 || qos_ == MqttQoS::LEVEL_0) {
    return;
  }

  {
    std::lock_guard lock{mutex_};
    --counter_;
  }
  cv_.notify_one();
}

}  // namespace org::apache::nifi::minifi::processors
