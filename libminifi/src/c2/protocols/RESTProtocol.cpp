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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

const C2Payload RESTProtocol::parseJsonResponse(const C2Payload &payload, const std::vector<char> &response) {
  Json::Reader reader;
  Json::Value root;
  try {
    if (reader.parse(std::string(response.data(), response.size()), root)) {
      std::string requested_operation = getOperation(payload);

      std::string identifier;
      if (root.isMember("operationid")) {
        identifier = root["operationid"].asString();
      }
      if (root["operation"].asString() == requested_operation) {
        if (root["requested_operations"].size() == 0) {
          return std::move(C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE, true));
        }
        C2Payload new_payload(payload.getOperation(), state::UpdateState::NESTED, true);

        new_payload.setIdentifier(identifier);

        for (const Json::Value& request : root["requested_operations"]) {
          Operation newOp = stringToOperation(request["operation"].asString());
          C2Payload nested_payload(newOp, state::UpdateState::READ_COMPLETE, true);
          C2ContentResponse new_command(newOp);
          new_command.delay = 0;
          new_command.required = true;
          new_command.ttl = -1;
          // set the identifier if one exists
          if (request.isMember("operationid")) {
            new_command.ident = request["operationid"].asString();
            nested_payload.setIdentifier(new_command.ident);
          }
          new_command.name = request["name"].asString();

          if (request.isMember("content") && request["content"].size() > 0) {
            for (const auto &name : request["content"].getMemberNames()) {
              new_command.operation_arguments[name] = request["content"][name].asString();
            }
          }
          nested_payload.addContent(std::move(new_command));
          new_payload.addPayload(std::move(nested_payload));
        }
        // we have a response for this request
        return std::move(new_payload);
      }
    }
  } catch (...) {
  }
  return std::move(C2Payload(payload.getOperation(), state::UpdateState::READ_ERROR, true));
}

Json::Value RESTProtocol::serializeJsonPayload(Json::Value &json_root, const C2Payload &payload) {
  // get the name from the content
  Json::Value json_payload;
  std::map<std::string, std::vector<Json::Value>> children;
  for (const auto &nested_payload : payload.getNestedPayloads()) {
    Json::Value child_payload = serializeJsonPayload(json_payload, nested_payload);
    children[nested_payload.getLabel()].push_back(child_payload);
  }
  for (auto child_vector : children) {
    if (child_vector.second.size() > 1) {
      Json::Value children_json(Json::arrayValue);
      for (auto child : child_vector.second) {
        json_payload[child_vector.first] = child;
      }
    } else {
      if (child_vector.second.size() == 1) {
        if (child_vector.second.at(0).isMember(child_vector.first)) {
          json_payload[child_vector.first] = child_vector.second.at(0)[child_vector.first];
        } else {
          json_payload[child_vector.first] = child_vector.second.at(0);
        }
      }
    }
  }

  const std::vector<C2ContentResponse> &content = payload.getContent();
  for (const auto &payload_content : content) {
    Json::Value payload_content_values;
    bool use_sub_option = true;
    if (payload_content.op == payload.getOperation()) {
      for (auto content : payload_content.operation_arguments) {
        if (payload_content.operation_arguments.size() == 1 && payload_content.name == content.first) {
          json_payload[payload_content.name] = content.second;
          use_sub_option = false;
        } else {
          payload_content_values[content.first] = content.second;
        }
      }
    }
    if (use_sub_option)
      json_payload[payload_content.name] = payload_content_values;
  }
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
