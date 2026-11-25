// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <expected>
#include <map>
#include <string>
#include <system_error>
#include <utility>

#include "minifi-cpp/core/PropertyDefinition.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::mock {

class MockProperties {
 public:
  template<typename Key, typename Value>
  void emplace(Key&& key, Value&& value) {
    values_.emplace(std::forward<Key>(key), std::forward<Value>(value));
  }

  [[nodiscard]] std::expected<std::string, std::error_code> get(const core::PropertyReference& property_reference) const {
    if (const auto it = values_.find(property_reference.name); it != values_.end()) { return it->second; }
    if (property_reference.default_value) { return std::string{*property_reference.default_value}; }
    return std::unexpected{make_error_code(core::PropertyErrorCode::PropertyNotSet)};
  }

 private:
  std::map<std::string, std::string, std::less<>> values_;
};

}  // namespace org::apache::nifi::minifi::mock
