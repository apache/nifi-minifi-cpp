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

#include "AgentPrinter.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <list>
#include <string>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

AgentPrinter::AgentPrinter(std::string name, utils::Identifier uuid)
    : HeartBeatReporter(name, uuid),
      logger_(logging::LoggerFactory<AgentPrinter>::getLogger()) {
}

void AgentPrinter::initialize(const std::shared_ptr<core::controller::ControllerServiceProvider> &controller, const std::shared_ptr<state::StateMonitor> &updateSink,
                              const std::shared_ptr<Configure> &configure) {
  HeartBeatReporter::initialize(controller, updateSink, configure);
}
int16_t AgentPrinter::heartbeat(const C2Payload &payload) {
  std::string outputConfig = serializeJsonRootPayload(payload);
  return 0;
}

std::string AgentPrinter::serializeJsonRootPayload(const C2Payload& payload) {
  rapidjson::Document json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);
  rapidjson::Document::AllocatorType &alloc = json_payload.GetAllocator();

  rapidjson::Value opReqStrVal;
  std::string operation_request_str = getOperation(payload);
  opReqStrVal.SetString(operation_request_str.c_str(), operation_request_str.length(), alloc);
  json_payload.AddMember("operation", opReqStrVal, alloc);

  std::string operationid = payload.getIdentifier();
  if (operationid.length() > 0) {
    rapidjson::Value operationIdVal = getStringValue(operationid, alloc);
    json_payload.AddMember("operationid", operationIdVal, alloc);
  }

  mergePayloadContent(json_payload, payload, alloc);

  for (const auto &nested_payload : payload.getNestedPayloads()) {
    rapidjson::Value np_key = getStringValue(nested_payload.getLabel(), alloc);
    rapidjson::Value np_value = serializeJsonPayload(nested_payload, alloc);
    json_payload.AddMember(np_key, np_value, alloc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  json_payload.Accept(writer);
  return buffer.GetString();
}

rapidjson::Value AgentPrinter::serializeJsonPayload(const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) {
  // get the name from the content
  rapidjson::Value json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);

  std::map<std::string, std::list<rapidjson::Value*>> children;
  bool print = payload.getLabel() == "agentManifest";
  for (const auto &nested_payload : payload.getNestedPayloads()) {

    rapidjson::Value* child_payload = new rapidjson::Value(serializeJsonPayload(nested_payload, alloc));
    children[nested_payload.getLabel()].push_back(child_payload);

  }

  // child_vector is Pair<string, vector<Value*>>
  for (auto child_vector : children) {
    rapidjson::Value children_json;
    rapidjson::Value newMemberKey = getStringValue(child_vector.first, alloc);
    if (child_vector.second.size() > 1) {
      children_json.SetArray();
      for (auto child : child_vector.second)
        children_json.PushBack(child->Move(), alloc);
      if (json_payload.IsArray())
        json_payload.PushBack(children_json, alloc);
      else
        json_payload.AddMember(newMemberKey, children_json, alloc);
    } else if (child_vector.second.size() == 1) {
      rapidjson::Value* first = child_vector.second.front();

      if (first->IsObject() && first->HasMember(newMemberKey)) {
        if (json_payload.IsArray())
          json_payload.PushBack((*first)[newMemberKey].Move(), alloc);
        else
          json_payload.AddMember(newMemberKey, (*first)[newMemberKey].Move(), alloc);
      } else {
        if (json_payload.IsArray()) {
          json_payload.PushBack(first->Move(), alloc);
        } else {
          json_payload.AddMember(newMemberKey, first->Move(), alloc);
        }
      }
    }

    for (rapidjson::Value* child : child_vector.second)
      delete child;
  }

  mergePayloadContent(json_payload, payload, alloc);

  if (print) {

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    json_payload.Accept(writer);
    std::cout << buffer.GetString() << std::endl;
    std::exit(1);
  }

  return json_payload;
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
