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

#include <string>
#include <unordered_set>

#include "utils/StringUtils.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::c2 {

enum class FlowStatusQueryType {
  processor,
  connection,
  instance,
  systemdiagnostics
};

struct FlowStatusRequest {
  FlowStatusQueryType query_type;
  std::string identifier;
  std::unordered_set<std::string> options;

  explicit FlowStatusRequest(const std::string& query_string) {
    auto query_parameters = minifi::utils::string::splitAndTrimRemovingEmpty(query_string, ":");
    if (query_parameters.size() < 2) {
      throw std::invalid_argument("Invalid query string: " + query_string);
    }
    auto query_type_result = magic_enum::enum_cast<FlowStatusQueryType>(query_parameters[0]);
    if (!query_type_result) {
      throw std::invalid_argument("Invalid query type: " + query_parameters[0]);
    }
    query_type = *query_type_result;
    if (query_parameters.size() > 2) {
      identifier = query_parameters[1];
      auto option_vector = minifi::utils::string::splitAndTrimRemovingEmpty(query_parameters[2], ",");
      options = std::unordered_set<std::string>(option_vector.begin(), option_vector.end());
    } else {
      auto option_vector = minifi::utils::string::splitAndTrimRemovingEmpty(query_parameters[1], ",");
      options = std::unordered_set<std::string>(option_vector.begin(), option_vector.end());
    }
  }
};

}  // namespace org::apache::nifi::minifi::c2
