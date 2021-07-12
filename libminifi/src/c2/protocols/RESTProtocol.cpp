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

#ifdef WIN32
#pragma push_macro("GetObject")
#undef GetObject
#endif

#include "c2/protocols/RESTProtocol.h"

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/TypedValues.h"
#include "utils/gsl.h"

#ifdef GetObject
#undef GetObject
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {


AnnotatedValue parseAnnotatedValue(const rapidjson::Value& jsonValue) {
  AnnotatedValue result;
  if (jsonValue.IsObject() && jsonValue.HasMember("value")) {
    result = jsonValue["value"].GetString();
    for (const auto& annotation : jsonValue.GetObject()) {
      if (annotation.name.GetString() == std::string("value")) {
        continue;
      }
      result.annotations[annotation.name.GetString()] = parseAnnotatedValue(annotation.value);
    }
  } else if (jsonValue.IsBool()) {
    result = jsonValue.GetBool();
  } else {
    result = jsonValue.GetString();
  }
  return result;
}

const C2Payload RESTProtocol::parseJsonResponse(const C2Payload &payload, const std::vector<char> &response) {
  rapidjson::Document root;

  try {
    rapidjson::ParseResult ok = root.Parse(response.data(), response.size());
    if (ok) {
      std::string identifier;
      for (auto key : {"operationid", "operationId", "identifier"}) {
        if (root.HasMember(key)) {
          identifier = root[key].GetString();
          break;
        }
      }

      int size = 0;
      for (auto key : {"requested_operations", "requestedOperations"}) {
        if (root.HasMember(key)) {
          size = root[key].Size();
          break;
        }
      }

      // neither must be there. We don't want assign array yet and cause an assertion error
      if (size == 0)
        return C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE);

      C2Payload new_payload(payload.getOperation(), state::UpdateState::NESTED);
      if (!identifier.empty())
        new_payload.setIdentifier(identifier);

      auto array = root.HasMember("requested_operations") ? root["requested_operations"].GetArray() : root["requestedOperations"].GetArray();

      for (const rapidjson::Value& request : array) {
        Operation newOp = Operation::parse(request["operation"].GetString(), Operation::HEARTBEAT, false);
        C2Payload nested_payload(newOp, state::UpdateState::READ_COMPLETE);
        C2ContentResponse new_command(newOp);
        new_command.delay = 0;
        new_command.required = true;
        new_command.ttl = -1;

        // set the identifier if one exists
        for (auto key : {"operationid", "operationId", "identifier"}) {
          if (request.HasMember(key)) {
            if (request[key].IsNumber()) {
              new_command.ident = std::to_string(request[key].GetInt64());
            } else if (request[key].IsString()) {
              new_command.ident = request[key].GetString();
            } else {
              throw Exception(SITE2SITE_EXCEPTION, "Invalid type for " + std::string{key});
            }
            nested_payload.setIdentifier(new_command.ident);
            break;
          }
        }

        if (request.HasMember("name")) {
          new_command.name = request["name"].GetString();
        } else if (request.HasMember("operand")) {
          new_command.name = request["operand"].GetString();
        }

        for (auto key : {"content", "args"}) {
          if (request.HasMember(key) && request[key].IsObject()) {
            for (const auto &member : request[key].GetObject()) {
              new_command.operation_arguments[member.name.GetString()] = parseAnnotatedValue(member.value);
            }
            break;
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
  return C2Payload(payload.getOperation(), state::UpdateState::READ_COMPLETE);
}

RESTProtocol::RESTProtocol() : logger_(logging::LoggerFactory<RESTProtocol>::getLogger()) {}

void RESTProtocol::initialize(core::controller::ControllerServiceProvider* /*controller*/, const std::shared_ptr<Configure> &configure) {
  if (configure) {
    std::string value_str;
    if (configure->get("nifi.c2.rest.heartbeat.minimize.updates", "c2.rest.heartbeat.minimize.updates", value_str)) {
      auto opt_value = utils::StringUtils::toBool(value_str);
      if (!opt_value) {
        logger_->log_error("Cannot convert '%s' to bool for property '%s'", value_str, "nifi.c2.rest.heartbeat.minimize.updates");
        minimize_updates_ = false;
      } else {
        minimize_updates_ = opt_value.value();
      }
    }
  }
}

void RESTProtocol::serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) {
  if (!minimize_updates_ || (minimize_updates_ && !containsPayload(payload))) {
    rapidjson::Value value = serializeJsonPayload(payload, alloc);
    if (minimize_updates_) {
      nested_payloads_.insert(std::pair<std::string, C2Payload>(payload.getLabel(), payload));
    }
    target.AddMember(rapidjson::Value(payload.getLabel().c_str(), alloc), value, alloc);
  }
}

bool RESTProtocol::containsPayload(const C2Payload &o) {
  auto it = nested_payloads_.find(o.getLabel());
  if (it != nested_payloads_.end()) {
    return it->second == o;
  }
  return false;
}

#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
