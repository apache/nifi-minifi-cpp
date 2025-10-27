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
#include "SplitJson.h"

#include <unordered_map>

#include "core/ProcessSession.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/Id.h"

#include "jsoncons_ext/jsonpath/jsonpath.hpp"

namespace org::apache::nifi::minifi::processors {

void SplitJson::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void SplitJson::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  json_path_expression_ = utils::parseProperty(context, SplitJson::JsonPathExpression);
  null_value_representation_ = utils::parseEnumProperty<split_json::NullValueRepresentationOption>(context, SplitJson::NullValueRepresentation);
}

std::optional<jsoncons::json> SplitJson::queryArrayUsingJsonPath(core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file) const {
  const auto json_string = to_string(session.readBuffer(flow_file));
  if (json_string.empty()) {
    logger_->log_error("FlowFile content is empty, transferring to the 'failure' relationship");
    return std::nullopt;
  }

  jsoncons::json json_object;
  try {
    json_object = jsoncons::json::parse(json_string);
  } catch (const jsoncons::json_exception& e) {
    logger_->log_error("FlowFile content is not a valid JSON document, transferring to the 'failure' relationship: {}", e.what());
    return std::nullopt;
  }

  jsoncons::json query_result;
  try {
    query_result = jsoncons::jsonpath::json_query(json_object, json_path_expression_);
  } catch (const jsoncons::jsonpath::jsonpath_error& e) {
    logger_->log_error("Invalid JSON path expression '{}' set in the 'JsonPath Expression' property: {}", json_path_expression_, e.what());
    return std::nullopt;
  }

  return query_result;
}

std::string SplitJson::jsonValueToString(const jsoncons::json& json_value) const {
  if (json_value.is_null()) {
    return null_value_representation_ == split_json::NullValueRepresentationOption::EmptyString ? "" : "null";
  }

  if (json_value.is_string()) {
    return json_value.as<std::string>();
  }

  return json_value.to_string();
}

void SplitJson::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto query_result = queryArrayUsingJsonPath(session, flow_file);
  if (!query_result) {
    session.transfer(flow_file, Failure);
    return;
  }

  gsl_Assert(query_result->is_array());
  if (query_result->empty()) {
    logger_->log_error("JSON Path expression '{}' did not match the input flow file content, transferring to the 'failure' relationship", json_path_expression_);
    session.transfer(flow_file, Failure);
    return;
  }

  if (query_result->size() == 1 && !query_result.value()[0].is_array()) {
    logger_->log_error("JSON Path expression '{}' did not return an array, transferring to the 'failure' relationship", json_path_expression_);
    session.transfer(flow_file, Failure);
    return;
  }

  const auto fragment_id = utils::IdGenerator::getIdGenerator()->generate().to_string();
  const jsoncons::json& result_array = query_result->size() == 1 ? query_result.value()[0] : *query_result;
  const auto original_filename = flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME);

  for (size_t i = 0; i < result_array.size(); ++i) {
    auto child_flow_file = session.create(flow_file.get());
    child_flow_file->setAttribute(core::SpecialFlowAttribute::FILENAME, child_flow_file->getUUIDStr());
    child_flow_file->setAttribute(SplitJson::FragmentIndex.name, std::to_string(i));
    child_flow_file->setAttribute(SplitJson::FragmentCount.name, std::to_string(result_array.size()));
    child_flow_file->setAttribute(SplitJson::FragmentIdentifier.name, fragment_id);
    child_flow_file->setAttribute(SplitJson::SegmentOriginalFilename.name,  original_filename ? original_filename.value() : "");

    auto& json_value_to_write = result_array[i];
    session.write(child_flow_file, [this, &json_value_to_write](const std::shared_ptr<io::OutputStream>& output_stream) -> int64_t {
      auto result_string = jsonValueToString(json_value_to_write);
      return gsl::narrow<int64_t>(output_stream->write(reinterpret_cast<const uint8_t*>(result_string.data()), result_string.size()));
    });

    session.transfer(child_flow_file, Split);
  }

  flow_file->setAttribute(SplitJson::FragmentIdentifier.name, fragment_id);
  flow_file->setAttribute(SplitJson::FragmentCount.name, std::to_string(result_array.size()));
  session.transfer(flow_file, Original);
}

REGISTER_RESOURCE(SplitJson, Processor);

}  // namespace org::apache::nifi::minifi::processors
