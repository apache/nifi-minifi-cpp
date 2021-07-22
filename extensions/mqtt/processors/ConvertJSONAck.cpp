/**
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
#include "ConvertJSONAck.h"

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
#include "c2/PayloadSerializer.h"
#include "utils/ByteArrayCallback.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {


std::string ConvertJSONAck::parseTopicName(const std::string &json) {
  std::string topic;
  rapidjson::Document root;

  try {
    rapidjson::ParseResult ok = root.Parse(json.c_str());
    if (ok) {
      if (root.HasMember("agentInfo")) {
        if (root["agentInfo"].HasMember("identifier")) {
          std::stringstream topicStr;
          topicStr << root["agentInfo"]["identifier"].GetString() << "/in";
          return topicStr.str();
        }
      }
    }
  } catch (...) {
  }
  return topic;
}
void ConvertJSONAck::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  if (nullptr == mqtt_service_) {
    context->yield();
    return;
  }
  auto flow = session->get();

  if (!flow) {
    return;
  }

  /**
   * This processor expects a JSON response from InvokeHTTP and thus we expect a heartbeat ack following that.
   * Since we are trailing InvokeHTTP
   */
  std::string topic;
  {
    // expect JSON response from InvokeHTTP and thus we expect a heartbeat and then the output from the HTTP
    c2::C2Payload response_payload(c2::Operation::HEARTBEAT, state::UpdateState::READ_COMPLETE, true);
    ReadCallback callback;
    session->read(flow, &callback);

    topic = parseTopicName(std::string(callback.buffer_.data(), callback.buffer_.size()));

    session->transfer(flow, Success);
  }
  flow = session->get();

  if (!flow) {
    return;
  }

  if (!topic.empty()) {
    ReadCallback callback;
    session->read(flow, &callback);

    c2::C2Payload response_payload(c2::Operation::HEARTBEAT, state::UpdateState::READ_COMPLETE, true);

    std::string str(callback.buffer_.data(), callback.buffer_.size());
    auto payload = parseJsonResponse(response_payload, callback.buffer_);

    auto stream = c2::PayloadSerializer::serialize(1, payload);

    mqtt_service_->send(topic, stream->getBuffer(), stream->size());
  }

  session->transfer(flow, Success);
}

REGISTER_INTERNAL_RESOURCE(ConvertJSONAck);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
