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

#include <utility>

#include "core/PropertyDefinition.h"
#include "core/PropertyType.h"
#include "core/ClassName.h"

namespace org::apache::nifi::minifi::core {

namespace detail {
template<typename... Types>
inline constexpr auto TypeNames = std::array<std::string_view, sizeof...(Types)>{core::className<Types>()...};
}

template<size_t NumAllowedValues = 0, size_t NumDependentProperties = 0, size_t NumExclusiveOfProperties = 0>
struct PropertyDefinitionBuilder {
  static constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> createProperty(std::string_view name) {
    PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> builder;
    builder.property.name = name;
    return builder;
  }

  static constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> createProperty(std::string_view name, std::string_view display_name) {
    PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> builder;
    builder.property.name = name;
    builder.property.display_name = display_name;
    return builder;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withDescription(std::string_view description) {
    property.description = description;
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> isRequired(bool required) {
    property.is_required = required;
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> isSensitive(bool sensitive) {
    property.is_sensitive = sensitive;
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> supportsExpressionLanguage(bool supports_expression_language) {
    property.supports_expression_language = supports_expression_language;
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withDefaultValue(std::string_view default_value) {
    property.default_value = std::optional<std::string_view>{default_value};  // workaround for gcc 11.1; on gcc 11.3 and later, `property.default_value = default_value` works, too
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withAllowedValues(
      std::array<std::string_view, NumAllowedValues> allowed_values) {
    property.allowed_values = allowed_values;
    return *this;
  }

  template<typename... AllowedTypes>
  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withAllowedTypes() {
    property.allowed_types = {detail::TypeNames<AllowedTypes...>};
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withDependentProperties(
      std::array<std::string_view, NumDependentProperties> dependent_properties) {
    property.dependent_properties = dependent_properties;
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withExclusiveOfProperties(
      std::array<std::pair<std::string_view, std::string_view>, NumExclusiveOfProperties> exclusive_of_properties) {
    property.exclusive_of_properties = exclusive_of_properties;
    return *this;
  }

  constexpr PropertyDefinitionBuilder<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> withPropertyType(const PropertyType& property_type) {
    property.type = gsl::make_not_null(&property_type);
    return *this;
  }

  constexpr PropertyDefinition<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> build() {
    return property;
  }

  PropertyDefinition<NumAllowedValues, NumDependentProperties, NumExclusiveOfProperties> property{
    .name = {},
    .display_name = {},
    .description = {},
    .is_required = false,
    .is_sensitive = false,
    .allowed_values = {},
    .allowed_types = {},
    .dependent_properties = {},
    .exclusive_of_properties = {},
    .default_value = {},
    .type = gsl::make_not_null(&StandardPropertyTypes::VALID_TYPE),
    .supports_expression_language = false,
    .version = 1
  };
};

}  // namespace org::apache::nifi::minifi::core
