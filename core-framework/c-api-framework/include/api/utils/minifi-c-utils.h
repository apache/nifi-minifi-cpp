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

#include "minifi-c.h"
#include <string_view>
#include "minifi-cpp/core/Annotation.h"
#include "utils/gsl.h"
#include "core/ClassName.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/core/PropertyDefinition.h"

namespace org::apache::nifi::minifi::api::utils {

inline MinifiStringView toStringView(std::string_view str) {
  return MinifiStringView{.data = str.data(), .length = gsl::narrow<uint32_t>(str.length())};
}

template<typename T>
std::string classNameWithDots() {
  std::string class_name{minifi::core::className<T>()};
  return minifi::utils::string::replaceAll(class_name, "::", ".");
}

inline MinifiStandardPropertyValidator toStandardPropertyValidator(const minifi::core::PropertyValidator* validator) {
  if (validator == &minifi::core::StandardPropertyValidators::ALWAYS_VALID_VALIDATOR) {
    return MINIFI_ALWAYS_VALID_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::NON_BLANK_VALIDATOR) {
    return MINIFI_NON_BLANK_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR) {
    return MINIFI_TIME_PERIOD_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR) {
    return MINIFI_BOOLEAN_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::INTEGER_VALIDATOR) {
    return MINIFI_INTEGER_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR) {
    return MINIFI_UNSIGNED_INTEGER_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::DATA_SIZE_VALIDATOR) {
    return MINIFI_DATA_SIZE_VALIDATOR;
  }
  if (validator == &minifi::core::StandardPropertyValidators::PORT_VALIDATOR) {
    return MINIFI_PORT_VALIDATOR;
  }
  gsl_FailFast();
}

inline MinifiInputRequirement toInputRequirement(minifi::core::annotation::Input req) {
  switch (req) {
    case minifi::core::annotation::Input::INPUT_REQUIRED: return MINIFI_INPUT_REQUIRED;
    case minifi::core::annotation::Input::INPUT_ALLOWED: return MINIFI_INPUT_ALLOWED;
    case minifi::core::annotation::Input::INPUT_FORBIDDEN: return MINIFI_INPUT_FORBIDDEN;
  }
  gsl_FailFast();
}

inline std::vector<MinifiProperty> toProperties(std::span<const minifi::core::PropertyReference> props, std::vector<std::vector<MinifiStringView>>& cache) {
  std::vector<MinifiProperty> properties;
    for (auto& prop : props) {
      std::vector<MinifiStringView> sv_cache;
      const size_t dependent_properties_begin = 0;
      for (auto& dep_prop : prop.dependent_properties) {
        sv_cache.emplace_back(toStringView(dep_prop));
      }
      const size_t exclusive_of_property_names_begin = sv_cache.size();
      for (auto& excl_of_prop : prop.exclusive_of_properties) {
        sv_cache.emplace_back(toStringView(excl_of_prop.first));
      }
      const size_t exclusive_of_property_values_begin = sv_cache.size();
      for (auto& excl_of_prop : prop.exclusive_of_properties) {
        sv_cache.emplace_back(toStringView(excl_of_prop.second));
      }
      const size_t allowed_values_begin = sv_cache.size();
      for (auto& allowed_value : prop.allowed_values) {
        sv_cache.emplace_back(toStringView(allowed_value));
      }
      const size_t allowed_types_begin = sv_cache.size();
      for (auto& allowed_type : prop.allowed_types) {
        sv_cache.emplace_back(toStringView(allowed_type));
      }
      const std::optional<size_t> default_value_begin = [&] () -> std::optional<size_t> {
        if (prop.default_value) {
          sv_cache.emplace_back(toStringView(*prop.default_value));
          return sv_cache.size() - 1;
        }
        return std::nullopt;
      }();
      properties.push_back(MinifiProperty{
        .name = toStringView(prop.name),
        .display_name = toStringView(prop.display_name),
        .description = toStringView(prop.description),
        .is_required = prop.is_required ? MINIFI_TRUE : MINIFI_FALSE,
        .is_sensitive = prop.is_sensitive ? MINIFI_TRUE : MINIFI_FALSE,
        .dependent_properties_count = gsl::narrow<uint32_t>(prop.dependent_properties.size()),
        .dependent_properties_ptr = &sv_cache[dependent_properties_begin],
        .exclusive_of_properties_count = gsl::narrow<uint32_t>(prop.exclusive_of_properties.size()),
        .exclusive_of_property_names_ptr = &sv_cache[exclusive_of_property_names_begin],
        .exclusive_of_property_values_ptr = &sv_cache[exclusive_of_property_values_begin],

        .default_value = default_value_begin ? &sv_cache[default_value_begin.value()] : nullptr,
        .allowed_values_count = gsl::narrow<uint32_t>(prop.allowed_values.size()),
        .allowed_values_ptr = &sv_cache[allowed_values_begin],
        .validator = MinifiGetStandardValidator(toStandardPropertyValidator(prop.validator)),

        .types_count = gsl::narrow<uint32_t>(prop.allowed_types.size()),
        .types_ptr = &sv_cache[allowed_types_begin],
        .supports_expression_language = prop.supports_expression_language ? MINIFI_TRUE : MINIFI_FALSE
      });
      cache.emplace_back(std::move(sv_cache));
    }
  return properties;
}

std::error_code make_error_code(MinifiStatus status);

}  // namespace org::apache::nifi::minifi::cpp::utils
