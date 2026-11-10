// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
