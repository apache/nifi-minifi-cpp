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

#include "c2/protocols/RESTProtocol.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include <vector>
#include <list>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

const C2Payload RESTProtocol::parseJsonResponse(const C2Payload &payload, const std::vector<char> &response) {
  rapidjson::Document root;

  try {
    rapidjson::ParseResult ok = root.Parse(response.data(), response.size());

    if (ok) {
      std::string requested_operation = getOperation(payload);

      std::string identifier;
      if (root.HasMember("operationid")) {
        identifier = root["operationid"].GetString();
      }

      if (root.HasMember("operation") && root["operation"].GetString() == requested_operation) {
        if (root["requested_operations"].Size() == 0)
          return std::move(C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true));

        C2Payload new_payload(payload.getOperation(), state::UpdateState::NESTED, true);

        new_payload.setIdentifier(identifier);

        for (const rapidjson::Value& request : root["requested_operations"].GetArray()) {
          Operation newOp = stringToOperation(request["operation"].GetString());
          C2Payload nested_payload(newOp, state::UpdateState::READ_COMPLETE, true);
          C2ContentResponse new_command(newOp);
          new_command.delay = 0;
          new_command.required = true;
          new_command.ttl = -1;

          // set the identifier if one exists
          if (request.HasMember("operationid")) {
            if (request["operationid"].IsNumber())
              new_command.ident = std::to_string(request["operationid"].GetInt64());
            else if (request["operationid"].IsString())
              new_command.ident = request["operationid"].GetString();
            else
              throw(Exception(SITE2SITE_EXCEPTION, "Invalid type for operationid"));

            nested_payload.setIdentifier(new_command.ident);
          }

          new_command.name = request["name"].GetString();

          if (request.HasMember("content") && request["content"].MemberCount() > 0)
            for (const auto &member : request["content"].GetObject())
              new_command.operation_arguments[member.name.GetString()] = member.value.GetString();

          nested_payload.addContent(std::move(new_command));
          new_payload.addPayload(std::move(nested_payload));
        }

        // we have a response for this request
        return new_payload;
      }
    }
  } catch (...) {
  }
  return std::move(C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR, true));
}

void setJsonStr(const std::string& key, const std::string& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) { // NOLINT
  rapidjson::Value keyVal;
  rapidjson::Value valueVal;
  const char* c_key = key.c_str();
  const char* c_val = value.c_str();

  keyVal.SetString(c_key, key.length(), alloc);
  valueVal.SetString(c_val, value.length(), alloc);

  parent.AddMember(keyVal, valueVal, alloc);
}

rapidjson::Value getStringValue(const std::string& value, rapidjson::Document::AllocatorType& alloc) { // NOLINT
  rapidjson::Value Val;
  Val.SetString(value.c_str(), value.length(), alloc);
  return Val;
}

void RESTProtocol::mergePayloadContent(rapidjson::Value &target, const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) {
  const std::vector<C2ContentResponse> &content = payload.getContent();

  for (const auto &payload_content : content) {
    rapidjson::Value payload_content_values(rapidjson::kObjectType);
    bool use_sub_option = true;

    if (payload_content.op == payload.getOperation()) {
      for (auto content : payload_content.operation_arguments) {
        if (payload_content.operation_arguments.size() == 1 && payload_content.name == content.first) {
          setJsonStr(payload_content.name, content.second, target, alloc);
          use_sub_option = false;
        } else {
          setJsonStr(content.first, content.second, payload_content_values, alloc);
        }
      }
    }
    if (use_sub_option) {
      rapidjson::Value sub_key = getStringValue(payload_content.name, alloc);
      target.AddMember(sub_key, payload_content_values, alloc);
    }
  }
}

std::string RESTProtocol::serializeJsonRootPayload(const C2Payload& payload) {
  rapidjson::Document json_payload(rapidjson::kObjectType);
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

  // std::string ret = ;
  return buffer.GetString();
}

rapidjson::Value RESTProtocol::serializeJsonPayload(const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) {
  // get the name from the content
  rapidjson::Value json_payload(rapidjson::kObjectType);

  std::map<std::string, std::list<rapidjson::Value*>> children;

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

      json_payload.AddMember(newMemberKey, children_json, alloc);
    } else if (child_vector.second.size() == 1) {
      rapidjson::Value* first = child_vector.second.front();

      if (first->IsObject() && first->HasMember(newMemberKey))
        json_payload.AddMember(newMemberKey, (*first)[newMemberKey].Move(), alloc);
      else
        json_payload.AddMember(newMemberKey, first->Move(), alloc);
    }

    for (rapidjson::Value* child : child_vector.second)
      delete child;
  }

  mergePayloadContent(json_payload, payload, alloc);

  return json_payload;
}

std::string RESTProtocol::getOperation(const C2Payload &payload) {
  switch (payload.getOperation()) {
    case Operation::ACKNOWLEDGE:
      return "acknowledge";
    case Operation::HEARTBEAT:
      return "heartbeat";
    case Operation::RESTART:
      return "restart";
    case Operation::DESCRIBE:
      return "describe";
    case Operation::STOP:
      return "stop";
    case Operation::START:
      return "start";
    case Operation::UPDATE:
      return "update";
    default:
      return "heartbeat";
  }
}

Operation RESTProtocol::stringToOperation(const std::string str) {
  std::string op = str;
  std::transform(str.begin(), str.end(), op.begin(), ::tolower);
  if (op == "heartbeat") {
    return Operation::HEARTBEAT;
  } else if (op == "acknowledge") {
    return Operation::ACKNOWLEDGE;
  } else if (op == "update") {
    return Operation::UPDATE;
  } else if (op == "describe") {
    return Operation::DESCRIBE;
  } else if (op == "restart") {
    return Operation::RESTART;
  } else if (op == "clear") {
    return Operation::CLEAR;
  } else if (op == "stop") {
    return Operation::STOP;
  } else if (op == "start") {
    return Operation::START;
  }
  return Operation::HEARTBEAT;
}

} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
