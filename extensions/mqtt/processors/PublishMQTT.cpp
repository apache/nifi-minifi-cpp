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
  std::string value;
  int64_t valInt;
  value = "";
  if (context->getProperty(MaxFlowSegSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    max_seg_size_ = valInt;
    logger_->log_debug("PublishMQTT: max flow segment size [%" PRIu64 "]", max_seg_size_);
  }

  retain_ = false;
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

  PublishMQTT::ReadCallback callback(flowFile->getSize(), max_seg_size_, topic_, client_, gsl::narrow<int>(qos_), retain_, delivered_token_);
  session->read(flowFile, std::ref(callback));
  if (callback.status_ < 0) {
    logger_->log_error("Failed to send flow to MQTT topic %s", topic_);
    session->transfer(flowFile, Failure);
  } else {
    logger_->log_debug("Sent flow with length %d to MQTT topic %s", callback.read_size_, topic_);
    session->transfer(flowFile, Success);
  }
}

}  // namespace org::apache::nifi::minifi::processors
