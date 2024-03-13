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

#include "core/flow/FlowSerializer.h"
#include "utils/OptionalUtils.h"

namespace org::apache::nifi::minifi::core::flow {

Overrides& Overrides::add(const utils::Identifier& component_id, std::string_view property_name, std::string_view property_value) {
  required_values_[component_id].emplace(property_name, property_value);
  return *this;
}

Overrides& Overrides::addOptional(const utils::Identifier& component_id, std::string_view property_name, std::string_view property_value) {
  optional_values_[component_id].emplace(property_name, property_value);
  return *this;
}

std::optional<std::string> Overrides::get(const utils::Identifier& component_id, std::string_view property_name) const {
  const auto get_optional = [&](const auto& container) -> std::optional<std::string> {
    if (container.contains(component_id) && container.at(component_id).contains(std::string{property_name})) {
      return {container.at(component_id).at(std::string{property_name})};
    } else {
      return std::nullopt;
    }
  };
  return get_optional(required_values_) | utils::orElse([&]{ return get_optional(optional_values_); });
}

std::vector<std::pair<std::string, std::string>> Overrides::getRequired(const utils::Identifier& component_id) const {
  if (required_values_.contains(component_id)) {
    return {begin(required_values_.at(component_id)), end(required_values_.at(component_id))};
  } else {
    return {};
  }
}

bool Overrides::isEmpty() const {
  return required_values_.empty() && optional_values_.empty();
}

}  // namespace org::apache::nifi::minifi::core::flow
