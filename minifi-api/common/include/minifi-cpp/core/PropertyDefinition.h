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

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "minifi-cpp/core/PropertyValidator.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::core {

template<size_t NumAllowedValues = 0>
struct PropertyDefinition {
  std::string_view name;
  std::string_view display_name;
  std::string_view description;
  bool is_required;
  bool is_sensitive;
  std::array<std::string_view, NumAllowedValues> allowed_values;
  std::span<const std::string_view> allowed_types;
  std::optional<std::string_view> default_value;
  gsl::not_null<const PropertyValidator*> validator;
  bool supports_expression_language;

  uint8_t version;
};

struct PropertyReference {
  std::string_view name;
  std::string_view display_name;
  std::string_view description;
  bool is_required = false;
  bool is_sensitive = false;
  std::span<const std::string_view> allowed_values;
  std::span<const std::string_view> allowed_types;
  std::optional<std::string_view> default_value;
  gsl::not_null<const PropertyValidator*> validator;
  bool supports_expression_language = false;

  template<size_t NumAllowedValues = 0>
  constexpr PropertyReference(const PropertyDefinition<NumAllowedValues>& property_definition)  // NOLINT: non-explicit on purpose
      : name{property_definition.name},
        display_name{property_definition.display_name},
        description{property_definition.description},
        is_required{property_definition.is_required},
        is_sensitive{property_definition.is_sensitive},
        allowed_values{property_definition.allowed_values},
        allowed_types{property_definition.allowed_types},
        default_value{property_definition.default_value},
        validator{property_definition.validator},
        supports_expression_language{property_definition.supports_expression_language} {}

  PropertyReference(const std::string_view name, const std::string_view display_name, const std::string_view description, const bool is_required, const bool is_sensitive,
      const std::span<const std::string_view> allowed_values, std::span<const std::string_view> allowed_types, std::optional<std::string_view> default_value,
      const gsl::not_null<const PropertyValidator*> validator, const bool supports_expression_language)
      : name(name),
        display_name(display_name),
        description(description),
        is_required(is_required),
        is_sensitive(is_sensitive),
        allowed_values(allowed_values),
        allowed_types(allowed_types),
        default_value(default_value),
        validator(validator),
        supports_expression_language(supports_expression_language) {}

  PropertyReference(const PropertyReference&) = default;
  PropertyReference(PropertyReference&&) = default;
  PropertyReference& operator=(PropertyReference&&) = default;
  PropertyReference& operator=(const PropertyReference&) = default;
  ~PropertyReference() = default;
};

}  // namespace org::apache::nifi::minifi::core
