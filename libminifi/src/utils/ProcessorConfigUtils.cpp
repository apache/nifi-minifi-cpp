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
#include "utils/ProcessorConfigUtils.h"

#include <vector>
#include <string>
#include <string_view>

#include "range/v3/algorithm/contains.hpp"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::utils {

std::vector<std::string> listFromCommaSeparatedProperty(const core::ProcessContext& context, std::string_view property_name) {
  std::string property_string;
  context.getProperty(property_name, property_string);
  return utils::string::splitAndTrim(property_string, ",");
}

std::vector<std::string> listFromRequiredCommaSeparatedProperty(const core::ProcessContext& context, std::string_view property_name) {
  return utils::string::splitAndTrim(getRequiredPropertyOrThrow(context, property_name), ",");
}

bool parseBooleanPropertyOrThrow(const core::ProcessContext& context, std::string_view property_name) {
  const std::string value_str = getRequiredPropertyOrThrow(context, property_name);
  const auto maybe_value = utils::string::toBool(value_str);
  if (!maybe_value) {
    throw std::runtime_error(std::string(property_name) + " property is invalid: value is " + value_str);
  }
  return maybe_value.value();
}

std::chrono::milliseconds parseTimePropertyMSOrThrow(const core::ProcessContext& context, std::string_view property_name) {
  const auto time_property = getRequiredPropertyOrThrow<core::TimePeriodValue>(context, property_name);
  return time_property.getMilliseconds();
}

}  // namespace org::apache::nifi::minifi::utils
