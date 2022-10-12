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
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ConsumeMQTT::enqueueReceivedMQTTMsg(std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter> message) {
  if (queue_.size_approx() >= maxQueueSize_) {
    logger_->log_warn("MQTT queue full");
    return;
  }

  if (gsl::narrow<uint64_t>(message->payloadlen) > max_seg_size_) {
    logger_->log_debug("MQTT message was truncated while enqueuing, original length: %d", message->payloadlen);
    message->payloadlen = gsl::narrow<int>(max_seg_size_);
  }

  logger_->log_debug("enqueuing MQTT message with length %d", message->payloadlen);
  queue_.enqueue(std::move(message));
}

void ConsumeMQTT::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
  if (const auto value = context->getProperty<bool>(CleanSession)) {
    cleanSession_ = *value;
    logger_->log_debug("ConsumeMQTT: CleanSession [%d]", cleanSession_);
  }

  if (const auto value = context->getProperty<uint64_t>(QueueBufferMaxMessage)) {
    maxQueueSize_ = *value;
    logger_->log_debug("ConsumeMQTT: Queue Max Message [%" PRIu64 "]", maxQueueSize_);
  }

  // this connects to broker, so properties of this processor must be read before
  AbstractMQTTProcessor::onSchedule(context, factory);
}

void ConsumeMQTT::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession> &session) {
  // reconnect if needed
  reconnect();

  if (!MQTTAsync_isConnected(client_)) {
    logger_->log_error("Could not consume from MQTT broker because disconnected to %s", uri_);
    yield();
    return;
  }

  std::deque<std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter>> msg_queue;
  getReceivedMQTTMsg(msg_queue);
  while (!msg_queue.empty()) {
    const auto& message = msg_queue.front();
    std::shared_ptr<core::FlowFile> processFlowFile = session->create();
    int write_status{};
    session->write(processFlowFile, [&message, &write_status](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
      if (message->payloadlen < 0) {
        write_status = -1;
        return -1;
      }
      const auto len = stream->write(reinterpret_cast<uint8_t*>(message->payload), gsl::narrow<size_t>(message->payloadlen));
      if (io::isError(len)) {
        write_status = -1;
        return -1;
      }
      return gsl::narrow<int64_t>(len);
    });
    if (write_status < 0) {
      logger_->log_error("ConsumeMQTT fail for the flow with UUID %s", processFlowFile->getUUIDStr());
      session->remove(processFlowFile);
    } else {
      session->putAttribute(processFlowFile, MQTT_BROKER_ATTRIBUTE, uri_);
      session->putAttribute(processFlowFile, MQTT_TOPIC_ATTRIBUTE, topic_);
      logger_->log_debug("ConsumeMQTT processing success for the flow with UUID %s topic %s", processFlowFile->getUUIDStr(), topic_);
      session->transfer(processFlowFile, Success);
    }
    msg_queue.pop_front();
  }
}

bool ConsumeMQTT::startupClient() {
  MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
  response_options.context = this;
  response_options.onSuccess = subscriptionSuccess;
  response_options.onFailure = subscriptionFailure;
  const int ret = MQTTAsync_subscribe(client_, topic_.c_str(), gsl::narrow<int>(qos_), &response_options);
  if (ret != MQTTASYNC_SUCCESS) {
    logger_->log_error("Failed to subscribe to MQTT topic %s (%d)", topic_, ret);
    return false;
  }
  logger_->log_debug("Successfully subscribed to MQTT topic: %s", topic_);
  return true;
}

void ConsumeMQTT::onMessageReceived(char* topic_name, int /*topic_len*/, MQTTAsync_message* message) {
  MQTTAsync_free(topic_name);

  const auto* msgPayload = reinterpret_cast<const char*>(message->payload);
  const size_t msgLen = message->payloadlen;
  const std::string messageText(msgPayload, msgLen);
  logger_->log_debug("Received message \"%s\" to MQTT topic %s on broker %s", messageText, topic_, uri_);

  std::unique_ptr<MQTTAsync_message, MQTTMessageDeleter> smartMessage(message);
  enqueueReceivedMQTTMsg(std::move(smartMessage));
}

void ConsumeMQTT::checkProperties() {
  if (!cleanSession_ && clientID_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Processor must have a Client ID for durable (non-clean) sessions");
  }
  if (!cleanSession_ && qos_ == 0) {
    logger_->log_warn("Messages are not preserved during client disconnection "
                      "by the broker when QoS is less than 1 for durable (non-clean) sessions. Only subscriptions are preserved.");
  }
}

}  // namespace org::apache::nifi::minifi::processors
