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

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::utils {

template<typename PropertyType = std::string>
PropertyType getRequiredPropertyOrThrow(const core::ProcessContext& context, std::string_view property_name) {
  PropertyType value;
  if (!context.getProperty(property_name, value)) {
    throw std::runtime_error(std::string(property_name) + " property missing or invalid");
  }
  return value;
}

std::vector<std::string> listFromCommaSeparatedProperty(const core::ProcessContext& context, std::string_view property_name);
std::vector<std::string> listFromRequiredCommaSeparatedProperty(const core::ProcessContext& context, std::string_view property_name);
bool parseBooleanPropertyOrThrow(const core::ProcessContext& context, std::string_view property_name);
std::chrono::milliseconds parseTimePropertyMSOrThrow(const core::ProcessContext& context, std::string_view property_name);
std::string parsePropertyWithAllowableValuesOrThrow(const core::ProcessContext& context, std::string_view property_name, std::span<const std::string_view> allowable_values);

template<typename T>
T parseEnumProperty(const core::ProcessContext& context, const core::Property& prop) {
  std::string value;
  if (!context.getProperty(prop.getName(), value)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + prop.getName() + "' is missing");
  }
  T result = T::parse(value.c_str(), T{});
  if (!result) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + prop.getName() + "' has invalid value: '" + value + "'");
  }
  return result;
}

template<typename T>
T parseEnumProperty(const core::ProcessContext& context, const core::PropertyReference& prop) {
  std::string value;
  if (!context.getProperty(prop.name, value)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + std::string(prop.name) + "' is missing");
  }
  T result = T::parse(value.c_str(), T{});
  if (!result) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + std::string(prop.name) + "' has invalid value: '" + value + "'");
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::utils
