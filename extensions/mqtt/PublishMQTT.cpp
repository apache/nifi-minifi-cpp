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
#include <stdio.h>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
#include <set>
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

void PublishMQTT::initialize() {
  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(BrokerURL);
  properties.insert(CleanSession);
  properties.insert(ClientID);
  properties.insert(UserName);
  properties.insert(PassWord);
  properties.insert(KeepLiveInterval);
  properties.insert(ConnectionTimeOut);
  properties.insert(QOS);
  properties.insert(Topic);
  properties.insert(Retain);
  properties.insert(MaxFlowSegSize);
  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

void PublishMQTT::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) {
  AbstractMQTTProcessor::onSchedule(context, sessionFactory);
  std::string value;
  int64_t valInt;
  value = "";
  if (context->getProperty(MaxFlowSegSize.getName(), value) && !value.empty() && core::Property::StringToInt(value, valInt)) {
    max_seg_size_ = valInt;
    logger_->log_debug("PublishMQTT: max flow segment size [%ll]", max_seg_size_);
  }
  value = "";
  if (context->getProperty(Retain.getName(), value) && !value.empty() && org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, retain_)) {
    logger_->log_debug("PublishMQTT: Retain [%d]", retain_);
  }
}

void PublishMQTT::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<core::FlowFile> flowFile = session->get();

  if (!flowFile) {
    return;
  }

  if (!reconnect()) {
    logger_->log_error("MQTT connect to %s failed", uri_);
    session->transfer(flowFile, Failure);
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
