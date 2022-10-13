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

void PublishMQTT::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void PublishMQTT::readProperties(const std::shared_ptr<core::ProcessContext>& context) {
  if (!context->getProperty(Topic).has_value()) {
    logger_->log_error("PublishMQTT: could not get Topic");
  }

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

void PublishMQTT::onTriggerImpl(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<core::FlowFile> flow_file = session->get();

  if (!flow_file) {
    return;
  }

  // broker's Receive Maximum can change after reconnect
  in_flight_message_counter_.setMax(broker_receive_maximum_.value_or(65535));

  const auto topic = getTopic(context, flow_file);
  PublishMQTT::ReadCallback callback(this, flow_file, topic, getContentType(context, flow_file));
  session->read(flow_file, std::ref(callback));
  if (!callback.getSuccessStatus()) {
    logger_->log_error("Failed to send flow file [%s] to MQTT topic '%s' on broker %s", flow_file->getUUIDStr(), topic, uri_);
    session->transfer(flow_file, Failure);
  } else {
    logger_->log_debug("Sent flow file [%s] with length %d to MQTT topic '%s' on broker %s", flow_file->getUUIDStr(), callback.getReadSize(), topic, uri_);
    session->transfer(flow_file, Success);
  }
}

int64_t PublishMQTT::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  if (flow_file_->getSize() > 268'435'455) {
    processor_->logger_->log_error("Sending message failed because MQTT limit maximum packet size [268'435'455] is exceeded by FlowFile of [%" PRIu64 "]", flow_file_->getSize());
    success_status_ = false;
    return -1;
  }

  if (processor_->maximum_packet_size_.has_value() && flow_file_->getSize() > *(processor_->maximum_packet_size_)) {
    processor_->logger_->log_error("Sending message failed because broker-requested maximum packet size [%" PRIu32 "] is exceeded by FlowFile of [%" PRIu64 "]",
                                   *processor_->maximum_packet_size_, flow_file_->getSize());
    success_status_ = false;
    return -1;
  }

  std::vector<std::byte> buffer(flow_file_->getSize());
  read_size_ = 0;
  success_status_ = true;
  while (read_size_ < flow_file_->getSize()) {
    gsl::span span(buffer.data() + read_size_, buffer.size() - read_size_);
    const auto readRet = stream->read(span);

    // read error
    if (io::isError(readRet)) {
      success_status_ = false;
      return -1;
    }

    // end of reading
    if (readRet == 0) {
      break;
    }

    read_size_ += readRet;
  }

  MQTTAsync_message message_to_publish = MQTTAsync_message_initializer;
  message_to_publish.payload = buffer.data();
  message_to_publish.payloadlen = gsl::narrow<int>(read_size_);
  message_to_publish.qos = processor_->qos_.value();
  message_to_publish.retained = processor_->retain_;

  setMqtt5Properties(message_to_publish);

  MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
  if (processor_->mqtt_version_ == MqttVersions::V_5_0) {
    response_options.onSuccess5 = sendSuccess5;
    response_options.onFailure5 = sendFailure5;
  } else {
    response_options.onSuccess = sendSuccess;
    response_options.onFailure = sendFailure;
  }

  // try sending the message
  if (!sendMessage(&message_to_publish, &response_options)) {
    // early fail
    success_status_ = false;
    return -1;
  }

  if (!success_status_) {
    return -1;
  }
  return gsl::narrow<int64_t>(read_size_);
}

bool PublishMQTT::ReadCallback::sendMessage(const MQTTAsync_message* message_to_publish, MQTTAsync_responseOptions* response_options) {
  // save context for callback
  std::packaged_task<void(bool, std::optional<int>, std::optional<MQTTReasonCodes>)> send_finished_task(
          [this] (const bool success, const std::optional<int> response_code, const std::optional<MQTTReasonCodes> reason_code) {
            notify(success, response_code, reason_code);
          });
  response_options->context = &send_finished_task;

  processor_->in_flight_message_counter_.increase();

  const int error_code = MQTTAsync_sendMessage(processor_->client_, topic_.c_str(), message_to_publish, response_options);
  if (error_code != MQTTASYNC_SUCCESS) {
    processor_->logger_->log_error("MQTTAsync_sendMessage failed on topic '%s', MQTT broker %s with error code [%d], flow file id [%s]",
                                   topic_, processor_->uri_, error_code, flow_file_->getUUIDStr());
    // early fail, sending attempt did not succeed, no need to wait for callback
    processor_->in_flight_message_counter_.decrease();
    return false;
  }

  send_finished_task.get_future().get();
  return true;
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
  if (retain_available_.has_value() && !*retain_available_ && retain_) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Retain was set but broker does not support it");
  }
}

void PublishMQTT::ReadCallback::sendSuccess(void* context, MQTTAsync_successData* /*response*/) {
  auto send_finished_task = reinterpret_cast<std::packaged_task<void(bool, std::optional<int>, std::optional<MQTTReasonCodes>)>*>(context);
  (*send_finished_task)(true, std::nullopt, std::nullopt);
}

void PublishMQTT::ReadCallback::sendSuccess5(void* context, MQTTAsync_successData5* response) {
  auto send_finished_task = reinterpret_cast<std::packaged_task<void(bool, std::optional<int>, std::optional<MQTTReasonCodes>)>*>(context);
  (*send_finished_task)(true, std::nullopt, response->reasonCode);
}

void PublishMQTT::ReadCallback::sendFailure(void* context, MQTTAsync_failureData* response) {
  auto send_finished_task = reinterpret_cast<std::packaged_task<void(bool, std::optional<int>, std::optional<MQTTReasonCodes>)>*>(context);
  (*send_finished_task)(false, response->code, std::nullopt);
}

void PublishMQTT::ReadCallback::sendFailure5(void* context, MQTTAsync_failureData5* response) {
  auto send_finished_task = reinterpret_cast<std::packaged_task<void(bool, std::optional<int>, std::optional<MQTTReasonCodes>)>*>(context);
  (*send_finished_task)(false, response->code, response->reasonCode);
}

void PublishMQTT::ReadCallback::notify(const bool success, const std::optional<int> response_code, const std::optional<MQTTReasonCodes> reason_code) {
  if (success) {
    processor_->logger_->log_debug("Successfully sent message to MQTT topic '%s' on broker %s", topic_, processor_->uri_);
    if (reason_code.has_value()) {
      processor_->logger_->log_error("Additional reason code for sending success: %d: %s", *reason_code, MQTTReasonCode_toString(*reason_code));
    }
  } else {
    processor_->logger_->log_error("Sending message failed on topic '%s' to MQTT broker %s with response code %d", topic_, processor_->uri_, *response_code);
    if (reason_code.has_value()) {
      processor_->logger_->log_error("Reason code for sending failure: %d: %s", *reason_code, MQTTReasonCode_toString(*reason_code));
    }
  }

  processor_->in_flight_message_counter_.decrease();
  if (!success) {
    success_status_ = false;
  }
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

void PublishMQTT::ReadCallback::setMqtt5Properties(MQTTAsync_message& message) const {
  if (processor_->mqtt_version_ != MqttVersions::V_5_0) {
    return;
  }

  if (processor_->message_expiry_interval_.has_value()) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_MESSAGE_EXPIRY_INTERVAL;
    property.value.integer4 = processor_->message_expiry_interval_->count();
    MQTTProperties_add(&message.properties, &property);
  }

  if (!content_type_.empty()) {
    MQTTProperty property;
    property.identifier = MQTTPROPERTY_CODE_CONTENT_TYPE;
    property.value.data.len = content_type_.length();
    property.value.data.data = const_cast<char*>(content_type_.data());
    MQTTProperties_add(&message.properties, &property);
  }

  addAttributesAsUserProperties(message);
}

void PublishMQTT::ReadCallback::addAttributesAsUserProperties(MQTTAsync_message& message) const {
  for (const auto& [key, value] : *flow_file_->getAttributesPtr()) {
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
