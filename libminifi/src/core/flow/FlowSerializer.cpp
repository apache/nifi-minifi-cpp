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

Overrides& Overrides::add(std::string_view property_name, std::string_view property_value) {
  overrides_.emplace(property_name, OverrideItem{.value = std::string{property_value}, .is_required = true});
  return *this;
}

Overrides& Overrides::addOptional(std::string_view property_name, std::string_view property_value) {
  overrides_.emplace(property_name, OverrideItem{.value = std::string{property_value}, .is_required = false});
  return *this;
}

std::optional<std::string> Overrides::get(std::string_view property_name) const {
  const auto it = overrides_.find(std::string{property_name});
  if (it != overrides_.end()) {
    return {it->second.value};
  } else {
    return std::nullopt;
  }
}

std::vector<std::pair<std::string, std::string>> Overrides::getRequired() const {
  std::vector<std::pair<std::string, std::string>> required_overrides;
  for (const auto& [name, override_item] : overrides_) {
    if (override_item.is_required) {
      required_overrides.emplace_back(name, override_item.value);
    }
  }
  return required_overrides;
}

}  // namespace org::apache::nifi::minifi::core::flow
