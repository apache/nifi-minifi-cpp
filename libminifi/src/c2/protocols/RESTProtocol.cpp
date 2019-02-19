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

#include "core/TypedValues.h"
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

#ifdef WIN32
#pragma push_macro("GetObject")
#undef GetObject
#endif
const C2Payload RESTProtocol::parseJsonResponse(const C2Payload &payload, const std::vector<char> &response) {
  rapidjson::Document root;

  try {
    rapidjson::ParseResult ok = root.Parse(response.data(), response.size());
    if (ok) {
      std::string requested_operation = getOperation(payload);

      std::string identifier;

      if (root.HasMember("operationid")) {
        identifier = root["operationid"].GetString();
      } else if (root.HasMember("operationId")) {
        identifier = root["operationId"].GetString();
      } else if (root.HasMember("identifier")) {
        identifier = root["identifier"].GetString();
      }

      int size = 0;
      if (root.HasMember("requested_operations")) {
        size = root["requested_operations"].Size();
      }
      if (root.HasMember("requestedOperations")) {
        size = root["requestedOperations"].Size();
      }

      // neither must be there. We don't want assign array yet and cause an assertion error
      if (size == 0)
        return C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true);

      C2Payload new_payload(payload.getOperation(), state::UpdateState::NESTED, true);
      if (!identifier.empty())
        new_payload.setIdentifier(identifier);

      auto array = root.HasMember("requested_operations") ? root["requested_operations"].GetArray() : root["requestedOperations"].GetArray();

      for (const rapidjson::Value& request : array) {
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
        } else if (request.HasMember("operationId")) {
          if (request["operationId"].IsNumber())
            new_command.ident = std::to_string(request["operationId"].GetInt64());
          else if (request["operationId"].IsString())
            new_command.ident = request["operationId"].GetString();
          else
            throw(Exception(SITE2SITE_EXCEPTION, "Invalid type for operationId"));
          nested_payload.setIdentifier(new_command.ident);
        } else if (request.HasMember("identifier")) {
          if (request["identifier"].IsNumber())
            new_command.ident = std::to_string(request["identifier"].GetInt64());
          else if (request["identifier"].IsString())
            new_command.ident = request["identifier"].GetString();
          else
            throw(Exception(SITE2SITE_EXCEPTION, "Invalid type for operationid"));
          nested_payload.setIdentifier(new_command.ident);
        }

        if (request.HasMember("name")) {
          new_command.name = request["name"].GetString();
        } else if (request.HasMember("operand")) {
          new_command.name = request["operand"].GetString();
        }

        if (request.HasMember("content") && request["content"].MemberCount() > 0) {
          if (request["content"].IsArray()) {
            for (const auto &member : request["content"].GetArray())
              new_command.operation_arguments[member.GetString()] = member.GetString();
          } else {
            for (const auto &member : request["content"].GetObject())
              new_command.operation_arguments[member.name.GetString()] = member.value.GetString();
          }
        } else if (request.HasMember("args") && request["args"].MemberCount() > 0) {
          if (request["args"].IsArray()) {
            for (const auto &member : request["args"].GetArray())
              new_command.operation_arguments[member.GetString()] = member.GetString();
          } else {
            for (const auto &member : request["args"].GetObject())
              new_command.operation_arguments[member.name.GetString()] = member.value.GetString();
          }
        }
        nested_payload.addContent(std::move(new_command));
        new_payload.addPayload(std::move(nested_payload));
      }

      // we have a response for this request
      return new_payload;
      // }
    }
  } catch (...) {
  }
  return C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true);
}

void setJsonStr(const std::string& key, const state::response::ValueNode& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) {  // NOLINT
  rapidjson::Value keyVal;
  rapidjson::Value valueVal;
  const char* c_key = key.c_str();
  auto base_type = value.getValue();
  keyVal.SetString(c_key, key.length(), alloc);

  auto type_index = base_type->getTypeIndex();
  if (auto sub_type = std::dynamic_pointer_cast<core::TransformableValue>(base_type)) {
    auto str = base_type->getStringValue();
    const char* c_val = str.c_str();
    valueVal.SetString(c_val, str.length(), alloc);
  } else {
    if (type_index == state::response::Value::BOOL_TYPE) {
      bool value = false;
      base_type->convertValue(value);
      valueVal.SetBool(value);
    } else if (type_index == state::response::Value::INT_TYPE) {
      int value = 0;
      base_type->convertValue(value);
      valueVal.SetInt(value);
    } else if (type_index == state::response::Value::INT64_TYPE) {
      int64_t value = 0;
      base_type->convertValue(value);
      valueVal.SetInt64(value);
    } else if (type_index == state::response::Value::UINT64_TYPE) {
      int64_t value = 0;
      base_type->convertValue(value);
      valueVal.SetInt64(value);
    } else {
      auto str = base_type->getStringValue();
      const char* c_val = str.c_str();
      valueVal.SetString(c_val, str.length(), alloc);
    }
  }
  parent.AddMember(keyVal, valueVal, alloc);
}

rapidjson::Value RESTProtocol::getStringValue(const std::string& value, rapidjson::Document::AllocatorType& alloc) {  // NOLINT
  rapidjson::Value Val;
  Val.SetString(value.c_str(), value.length(), alloc);
  return Val;
}

void RESTProtocol::mergePayloadContent(rapidjson::Value &target, const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) {
  const std::vector<C2ContentResponse> &content = payload.getContent();
  bool all_empty = !content.empty();
  bool is_parent_array = target.IsArray();

  for (const auto &payload_content : content) {
    for (const auto &op_arg : payload_content.operation_arguments) {
      if (!op_arg.second.empty()) {
        all_empty = false;
        break;
      }
    }
    if (!all_empty)
      break;
  }

  if (all_empty) {
    if (!is_parent_array) {
      target.SetArray();
      is_parent_array = true;
    }
    rapidjson::Value arr(rapidjson::kArrayType);
    for (const auto &payload_content : content) {
      for (const auto& op_arg : payload_content.operation_arguments) {
        rapidjson::Value keyVal;
        keyVal.SetString(op_arg.first.c_str(), op_arg.first.length(), alloc);
        if (is_parent_array) {
          target.PushBack(keyVal, alloc);
        } else {
          arr.PushBack(keyVal, alloc);
        }
      }
    }

    if (!is_parent_array) {
      rapidjson::Value sub_key = getStringValue(payload.getLabel(), alloc);
      target.AddMember(sub_key, arr, alloc);
    }
    return;
  }
  for (const auto &payload_content : content) {
    rapidjson::Value payload_content_values(rapidjson::kObjectType);
    bool use_sub_option = true;
    if (payload_content.op == payload.getOperation()) {
      for (const auto& op_arg : payload_content.operation_arguments) {
        if (!op_arg.second.empty()) {
          setJsonStr(op_arg.first, op_arg.second, target, alloc);
        }
      }
    } else {
    }
    if (use_sub_option) {
      rapidjson::Value sub_key = getStringValue(payload_content.name, alloc);
    }
  }
}

std::string RESTProtocol::serializeJsonRootPayload(const C2Payload& payload) {
  rapidjson::Document json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);
  rapidjson::Document::AllocatorType &alloc = json_payload.GetAllocator();

  rapidjson::Value opReqStrVal;
  std::string operation_request_str = getOperation(payload);
  opReqStrVal.SetString(operation_request_str.c_str(), operation_request_str.length(), alloc);
  json_payload.AddMember("operation", opReqStrVal, alloc);

  std::string operationid = payload.getIdentifier();
  if (operationid.length() > 0) {
    json_payload.AddMember("operationid", getStringValue(operationid, alloc), alloc);
    json_payload.AddMember("operationId", getStringValue(operationid, alloc), alloc);
    json_payload.AddMember("identifier", getStringValue(operationid, alloc), alloc);
  }

  mergePayloadContent(json_payload, payload, alloc);

  for (const auto &nested_payload : payload.getNestedPayloads()) {
    if (!minimize_updates_ || (minimize_updates_ && !containsPayload(nested_payload))) {
      rapidjson::Value np_key = getStringValue(nested_payload.getLabel(), alloc);
      rapidjson::Value np_value = serializeJsonPayload(nested_payload, alloc);
      if (minimize_updates_) {
        nested_payloads_.insert(std::pair<std::string, C2Payload>(nested_payload.getLabel(), nested_payload));
      }
      json_payload.AddMember(np_key, np_value, alloc);
    }
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  json_payload.Accept(writer);
  return buffer.GetString();
}

bool RESTProtocol::containsPayload(const C2Payload &o) {
  auto it = nested_payloads_.find(o.getLabel());
  if (it != nested_payloads_.end()) {
    return it->second == o;
  }
  return false;
}

rapidjson::Value RESTProtocol::serializeJsonPayload(const C2Payload &payload, rapidjson::Document::AllocatorType &alloc) {
// get the name from the content
  rapidjson::Value json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);

  std::vector<ValueObject> children;

  for (const auto &nested_payload : payload.getNestedPayloads()) {
    rapidjson::Value* child_payload = new rapidjson::Value(serializeJsonPayload(nested_payload, alloc));

    if (nested_payload.isCollapsible()) {
      bool combine = false;
      for (auto &subordinate : children) {
        if (subordinate.name == nested_payload.getLabel()) {
          subordinate.values.push_back(child_payload);
          combine = true;
          break;
        }
      }
      if (!combine) {
        ValueObject obj;
        obj.name = nested_payload.getLabel();
        obj.values.push_back(child_payload);
        children.push_back(obj);
      }
    } else {
      ValueObject obj;
      obj.name = nested_payload.getLabel();
      obj.values.push_back(child_payload);
      children.push_back(obj);
    }
  }

  for (auto child_vector : children) {
    rapidjson::Value children_json;
    rapidjson::Value newMemberKey = getStringValue(child_vector.name, alloc);
    if (child_vector.values.size() > 1) {
      children_json.SetArray();
      for (auto child : child_vector.values) {
        if (json_payload.IsArray())
          json_payload.PushBack(child->Move(), alloc);
        else
          children_json.PushBack(child->Move(), alloc);
      }
      if (!json_payload.IsArray())
        json_payload.AddMember(newMemberKey, children_json, alloc);
    } else if (child_vector.values.size() == 1) {
      rapidjson::Value* first = child_vector.values.front();
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
    for (rapidjson::Value* child : child_vector.values)
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
#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
} /* namespace c2 */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
