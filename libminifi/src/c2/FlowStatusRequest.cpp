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
#include "c2/FlowStatusRequest.h"

#include "utils/StringUtils.h"
#include "utils/Enum.h"
#include "range/v3/view/transform.hpp"
#include "range/v3/range/conversion.hpp"

namespace org::apache::nifi::minifi::c2 {

FlowStatusRequest::FlowStatusRequest(std::string_view query_string) {
  auto query_parameters = minifi::utils::string::splitAndTrim(query_string, ":");
  if (query_parameters.size() < 2) {
    throw std::invalid_argument(fmt::format("Invalid query string: {}", query_string));
  }
  auto query_type_result = magic_enum::enum_cast<FlowStatusQueryType>(query_parameters[0]);
  if (!query_type_result) {
    throw std::invalid_argument(fmt::format("Invalid query type: {}", query_parameters[0]));
  }
  query_type = *query_type_result;
  std::vector<std::string> option_vector_strings;
  if (query_parameters.size() > 2) {
    identifier = query_parameters[1];
    option_vector_strings = minifi::utils::string::splitAndTrimRemovingEmpty(query_parameters[2], ",");
  } else {
    option_vector_strings = minifi::utils::string::splitAndTrimRemovingEmpty(query_parameters[1], ",");
  }

  options = (option_vector_strings
    | ranges::views::transform([](const std::string& option_string) {
        auto result = magic_enum::enum_cast<FlowStatusQueryOption>(option_string);
        if (!result) {
          throw std::invalid_argument("Invalid query option: " + option_string);
        }
        return *result;
      })
    | ranges::to<std::unordered_set<FlowStatusQueryOption>>());
}

}  // namespace org::apache::nifi::minifi::c2
