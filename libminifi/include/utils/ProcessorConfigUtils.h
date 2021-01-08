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

#pragma once

#include <vector>
#include <string>

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::string getRequiredPropertyOrThrow(const core::ProcessContext* context, const std::string& property_name) {
  std::string value;
  if (!context->getProperty(property_name, value)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, property_name + " property missing or invalid");
  }
  return value;
}

std::vector<std::string> listFromCommaSeparatedProperty(const core::ProcessContext* context, const std::string& property_name) {
  std::string property_string;
  context->getProperty(property_name, property_string);
  return utils::StringUtils::splitAndTrim(property_string, ",");
}

std::vector<std::string> listFromRequiredCommaSeparatedProperty(const core::ProcessContext* context, const std::string& property_name) {
  return utils::StringUtils::splitAndTrim(getRequiredPropertyOrThrow(context, property_name), ",");
}

bool parseBooleanPropertyOrThrow(core::ProcessContext* context, const std::string& property_name) {
  bool value;
  const std::string value_str = getRequiredPropertyOrThrow(context, property_name);
  utils::optional<bool> maybe_value = utils::StringUtils::toBool(value_str);
  if (!maybe_value) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, property_name + " property is invalid: value is " + value_str);
  }
  return maybe_value.value();
}

std::chrono::milliseconds parseTimePropertyMSOrThrow(core::ProcessContext* context, const std::string& property_name) {
  core::TimeUnit unit;
  uint64_t time_value_ms;
  const std::string value_str = getRequiredPropertyOrThrow(context, property_name);
  if (!core::Property::StringToTime(value_str, time_value_ms, unit) || !core::Property::ConvertTimeUnitToMS(time_value_ms, unit, time_value_ms)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, property_name + " property is invalid: value is " + value_str);
  }
  return std::chrono::milliseconds(time_value_ms);
}

utils::optional<uint64_t> getOptionalUintProperty(const core::ProcessContext& context, const std::string& property_name) {
  uint64_t value;
  if (context.getProperty(property_name, value)) {
    return { value };
  }
  return utils::nullopt;
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
