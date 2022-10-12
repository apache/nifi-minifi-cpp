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

#include <cinttypes>
#include <memory>
#include <optional>
#include <string>
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

void PublishMQTT::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
  if (const auto retain_opt = context->getProperty<bool>(Retain)) {
    retain_ = *retain_opt;
  }
  logger_->log_debug("PublishMQTT: Retain [%d]", retain_);

  AbstractMQTTProcessor::onSchedule(context, factory);
}

void PublishMQTT::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession> &session) {
  // reconnect if needed
  reconnect();

  if (!MQTTAsync_isConnected(client_)) {
    logger_->log_error("Could not publish to MQTT broker because disconnected to %s", uri_);
    yield();
    return;
  }

  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  PublishMQTT::ReadCallback callback(this, flowFile->getSize(), max_seg_size_, topic_, client_, gsl::narrow<int>(qos_), retain_);
  session->read(flowFile, std::ref(callback));
  if (callback.status_ < 0) {
    logger_->log_error("Failed to send flow to MQTT topic %s", topic_);
    session->transfer(flowFile, Failure);
  } else {
    logger_->log_debug("Sent flow with length %d to MQTT topic %s", callback.read_size_, topic_);
    session->transfer(flowFile, Success);
  }
}

int64_t PublishMQTT::ReadCallback::operator()(const std::shared_ptr<io::InputStream>& stream) {
  if (flow_size_ < max_seg_size_)
    max_seg_size_ = flow_size_;
  gsl_Expects(max_seg_size_ < gsl::narrow<uint64_t>(std::numeric_limits<int>::max()));
  std::vector<std::byte> buffer(max_seg_size_);
  read_size_ = 0;
  status_ = 0;
  while (read_size_ < flow_size_) {
    // MQTTClient_message::payloadlen is int, so we can't handle 2GB+
    const auto readRet = stream->read(buffer);
    if (io::isError(readRet)) {
      status_ = -1;
      return gsl::narrow<int64_t>(read_size_);
    }
    if (readRet > 0) {
      MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
      pubmsg.payload = buffer.data();
      pubmsg.payloadlen = gsl::narrow<int>(readRet);
      pubmsg.qos = qos_;
      pubmsg.retained = retain_;
      MQTTAsync_responseOptions response_options = MQTTAsync_responseOptions_initializer;
      response_options.context = processor_;
      response_options.onSuccess = PublishMQTT::sendSuccess;
      response_options.onFailure = PublishMQTT::sendFailure;
      if (MQTTAsync_sendMessage(client_, topic_.c_str(), &pubmsg, &response_options) != MQTTASYNC_SUCCESS) {
        status_ = -1;
        return -1;
      }
      read_size_ += gsl::narrow<size_t>(readRet);
    } else {
      break;
    }
  }
  return gsl::narrow<int64_t>(read_size_);
}

}  // namespace org::apache::nifi::minifi::processors
