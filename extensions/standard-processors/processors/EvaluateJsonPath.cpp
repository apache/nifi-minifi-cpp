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
#include "EvaluateJsonPath.h"

#include <unordered_map>

#include "core/ProcessSession.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

#include "jsoncons_ext/jsonpath/jsonpath.hpp"

namespace org::apache::nifi::minifi::processors {

namespace {
bool isScalar(const jsoncons::json& value) {
  return !value.is_array() && !value.is_object();
}

bool isQueryResultEmptyOrScalar(const jsoncons::json& query_result) {
  return query_result.empty() || (query_result.size() == 1 && isScalar(query_result[0]));
}
}  // namespace

void EvaluateJsonPath::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void EvaluateJsonPath::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  const auto dynamic_properties = context.getDynamicPropertyKeys();
  if (dynamic_properties.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "At least one dynamic property must be specified with a valid JSON path expression");
  }
  destination_ = utils::parseEnumProperty<evaluate_json_path::DestinationType>(context, EvaluateJsonPath::Destination);
  if (destination_ == evaluate_json_path::DestinationType::FlowFileContent && dynamic_properties.size() > 1) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Only one dynamic property is allowed for JSON path when destination is set to flowfile-content");
  }
  null_value_representation_ = utils::parseEnumProperty<evaluate_json_path::NullValueRepresentationOption>(context, EvaluateJsonPath::NullValueRepresentation);
  path_not_found_behavior_ = utils::parseEnumProperty<evaluate_json_path::PathNotFoundBehaviorOption>(context, EvaluateJsonPath::PathNotFoundBehavior);
  return_type_ = utils::parseEnumProperty<evaluate_json_path::ReturnTypeOption>(context, EvaluateJsonPath::ReturnType);
  if (return_type_ == evaluate_json_path::ReturnTypeOption::AutoDetect) {
    if (destination_ == evaluate_json_path::DestinationType::FlowFileContent) {
      return_type_ = evaluate_json_path::ReturnTypeOption::JSON;
    } else {
      return_type_ = evaluate_json_path::ReturnTypeOption::Scalar;
    }
  }
}

std::string EvaluateJsonPath::extractQueryResult(const jsoncons::json& query_result) const {
  gsl_Expects(!query_result.empty());
  if (query_result.size() > 1) {
    gsl_Assert(return_type_ == evaluate_json_path::ReturnTypeOption::JSON);
    return query_result.to_string();
  }

  if (query_result[0].is_null()) {
    return null_value_representation_ == evaluate_json_path::NullValueRepresentationOption::EmptyString ? "" : "null";
  }

  if (query_result[0].is_string()) {
    return query_result[0].as<std::string>();
  }

  return query_result[0].to_string();
}

void EvaluateJsonPath::writeQueryResult(core::ProcessSession& session, core::FlowFile& flow_file, const jsoncons::json& query_result, const std::string& property_name,
    std::unordered_map<std::string, std::string>& attributes_to_set) const {
  if (destination_ == evaluate_json_path::DestinationType::FlowFileContent) {
    session.write(flow_file, [&query_result, this](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
      auto result_string = extractQueryResult(query_result);
      return gsl::narrow<int64_t>(output_stream->write(reinterpret_cast<const uint8_t*>(result_string.data()), result_string.size()));
    });
  } else {
    attributes_to_set.emplace(property_name, extractQueryResult(query_result));
  }
}

void EvaluateJsonPath::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    logger_->log_debug("No FlowFile available, yielding");
    return;
  }

  const auto json_string = to_string(session.readBuffer(flow_file));
  if (json_string.empty()) {
    logger_->log_error("FlowFile content is empty, transferring to Failure relationship");
    session.transfer(flow_file, Failure);
    return;
  }

  jsoncons::json json_object;
  try {
    json_object = jsoncons::json::parse(json_string);
  } catch (const jsoncons::json_exception& e) {
    logger_->log_error("FlowFile content is not a valid JSON document, transferring to Failure relationship: {}", e.what());
    session.transfer(flow_file, Failure);
    return;
  }

  std::unordered_map<std::string, std::string> attributes_to_set;
  for (const auto& property_name : context.getDynamicPropertyKeys()) {
    const auto result = context.getRawDynamicProperty(property_name);
    if (!result) {
      logger_->log_error("Failed to retrieve dynamic property '{}' for FlowFile with UUID '{}', transferring to Failure relationship", property_name, flow_file->getUUIDStr());
      session.transfer(flow_file, Failure);
      return;
    }
    const auto& json_path = *result;
    jsoncons::json query_result;
    try {
      query_result = jsoncons::jsonpath::json_query(json_object, json_path);
    } catch (const jsoncons::jsonpath::jsonpath_error& e) {
      logger_->log_error("Invalid JSON path expression '{}' found for attribute key '{}': {}", json_path, property_name, e.what());
      session.transfer(flow_file, Failure);
      return;
    }

    if (!query_result.is_array() || query_result.empty()) {
      if (path_not_found_behavior_ == evaluate_json_path::PathNotFoundBehaviorOption::Warn) {
        logger_->log_warn("JSON path '{}' not found for attribute key '{}'", json_path, property_name);
      }

      if (destination_ == evaluate_json_path::DestinationType::FlowFileContent) {
        logger_->log_debug("JSON path '{}' not found for attribute key '{}', transferring to Unmatched relationship", json_path, property_name);
        session.transfer(flow_file, Unmatched);
        return;
      }

      if (path_not_found_behavior_ != evaluate_json_path::PathNotFoundBehaviorOption::Skip) {
        flow_file->setAttribute(property_name, "");
      }
      continue;
    }

    if (return_type_ == evaluate_json_path::ReturnTypeOption::Scalar && !isQueryResultEmptyOrScalar(query_result)) {
      logger_->log_error("JSON path '{}' returned a non-scalar value or multiple values for attribute key '{}', transferring to Failure relationship", json_path, property_name);
      session.transfer(flow_file, Failure);
      return;
    }

    writeQueryResult(session, *flow_file, query_result, property_name, attributes_to_set);
  }

  if (destination_ == evaluate_json_path::DestinationType::FlowFileAttribute) {
    for (const auto& [key, value] : attributes_to_set) {
      session.putAttribute(*flow_file, key, value);
    }
  }
  session.transfer(flow_file, Matched);
}

REGISTER_RESOURCE(EvaluateJsonPath, Processor);

}  // namespace org::apache::nifi::minifi::processors
