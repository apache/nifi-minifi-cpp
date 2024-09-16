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

#include "core/Core.h"
#include "core/PropertyType.h"
#include "utils/gsl.h"
#include "Property.h"

namespace org::apache::nifi::minifi::core {

template<size_t NumAllowedValues = 0, size_t NumDependentProperties = 0, size_t NumExclusiveOfProperties = 0>
struct PropertyDefinition {
  std::string_view name;
  std::string_view display_name;
  std::string_view description;
  bool is_required = false;
  bool is_sensitive = false;
  std::array<std::string_view, NumAllowedValues> allowed_values;
  std::span<const std::string_view> allowed_types;
  std::array<std::string_view, NumDependentProperties> dependent_properties;
  std::array<std::pair<std::string_view, std::string_view>, NumExclusiveOfProperties> exclusive_of_properties;
  std::optional<std::string_view> default_value;
  gsl::not_null<const PropertyType*> type{gsl::make_not_null(&StandardPropertyTypes::VALID_TYPE)};
  bool supports_expression_language = false;

  uint8_t version = 1;
};

struct PropertyReference {
  std::string_view name;
  std::string_view display_name;
  std::string_view description;
  bool is_required = false;
  bool is_sensitive = false;
  std::span<const std::string_view> allowed_values;
  std::span<const std::string_view> allowed_types;
  std::span<const std::string_view> dependent_properties;
  std::span<const std::pair<std::string_view, std::string_view>> exclusive_of_properties;
  std::optional<std::string_view> default_value;
  gsl::not_null<const PropertyType*> type{gsl::make_not_null(&StandardPropertyTypes::VALID_TYPE)};
  bool supports_expression_language = false;

  constexpr PropertyReference() = default;

  template<size_t NumAllowedValues = 0, size_t NumDependentProperties = 0, size_t NumExclusiveOfProperties = 0>
  constexpr PropertyReference(const PropertyDefinition<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties>& property_definition)  // NOLINT: non-explicit on purpose
    : name{property_definition.name},
      display_name{property_definition.display_name},
      description{property_definition.description},
      is_required{property_definition.is_required},
      is_sensitive{property_definition.is_sensitive},
      allowed_values{property_definition.allowed_values},
      allowed_types{property_definition.allowed_types},
      dependent_properties{property_definition.dependent_properties},
      exclusive_of_properties{property_definition.exclusive_of_properties},
      default_value{property_definition.default_value},
      type{property_definition.type},
      supports_expression_language{property_definition.supports_expression_language} {
  }

  explicit operator Property() const;
};

}  // namespace org::apache::nifi::minifi::core
