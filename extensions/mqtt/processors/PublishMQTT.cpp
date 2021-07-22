/**
 * @file PublishMQTT.cpp
 * PublishMQTT class implementation
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
#include "PublishMQTT.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property PublishMQTT::Retain("Retain", "Retain MQTT published record in broker", "false");
core::Property PublishMQTT::MaxFlowSegSize("Max Flow Segment Size", "Maximum flow content payload segment size for the MQTT record", "");

core::Relationship PublishMQTT::Success("success", "FlowFiles that are sent successfully to the destination are transferred to this relationship");
core::Relationship PublishMQTT::Failure("failure", "FlowFiles that failed to send to the destination are transferred to this relationship");

void PublishMQTT::initialize() {
  // Set the supported properties
  std::set<core::Property> properties(AbstractMQTTProcessor::getSupportedProperties());
  properties.insert(Retain);
  properties.insert(MaxFlowSegSize);
  setSupportedProperties(properties);
  // Set the supported relationships
  setSupportedRelationships({Success, Failure});
}

void PublishMQTT::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
  AbstractMQTTProcessor::onSchedule(context, factory);
  std::string value;
  int64_t valInt;
  value = "";
  if (context->getProperty(MaxFlowSegSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    max_seg_size_ = valInt;
    logger_->log_debug("PublishMQTT: max flow segment size [%" PRIu64 "]", max_seg_size_);
  }

  const auto retain_parsed = [&] () -> std::optional<bool> {
    std::string property_value;
    if (!context->getProperty(CleanSession.getName(), property_value)) return std::nullopt;
    return utils::StringUtils::toBool(property_value);
  }();
  if ( retain_parsed ) {
    retain_ = *retain_parsed;
    logger_->log_debug("PublishMQTT: Retain [%d]", retain_);
  }
}

void PublishMQTT::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession> &session) {
  if (!reconnect()) {
    logger_->log_error("MQTT connect to %s failed", uri_);
    yield();
    return;
  }
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  PublishMQTT::ReadCallback callback(flowFile->getSize(), max_seg_size_, topic_, client_, qos_, retain_, delivered_token_);
  session->read(flowFile, &callback);
  if (callback.status_ < 0) {
    logger_->log_error("Failed to send flow to MQTT topic %s", topic_);
    session->transfer(flowFile, Failure);
  } else {
    logger_->log_debug("Sent flow with length %d to MQTT topic %s", callback.read_size_, topic_);
    session->transfer(flowFile, Success);
  }
}

REGISTER_RESOURCE(PublishMQTT, "PublishMQTT serializes FlowFile content as an MQTT payload, sending the message to the configured topic and broker.");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
