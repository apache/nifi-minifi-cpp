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
#include "minifi-cpp/utils/gsl.h"
#include "core/ClassName.h"
#include "utils/StringUtils.h"
#include "minifi-cpp/core/PropertyDefinition.h"

namespace org::apache::nifi::minifi::api::utils {

inline MinifiStringView toStringView(std::string_view str) {
  return MinifiStringView{.data = str.data(), .length = str.length()};
}

template<typename T>
std::string classNameWithDots() {
  std::string class_name{minifi::core::className<T>()};
  return minifi::utils::string::replaceAll(class_name, "::", ".");
}

MinifiStandardPropertyValidator toStandardPropertyValidator(const minifi::core::PropertyValidator* validator);

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
      const size_t allowed_values_begin = sv_cache.size();
      for (auto& allowed_value : prop.allowed_values) {
        sv_cache.emplace_back(toStringView(allowed_value));
      }
      const std::optional<size_t> allowed_types_begin = [&] () -> std::optional<size_t> {
        if (prop.allowed_types.empty()) {
          return std::nullopt;
        }
        gsl_Expects(prop.allowed_types.size() == 1);
        sv_cache.emplace_back(toStringView(prop.allowed_types[0]));
        return sv_cache.size() - 1;
      }();
      const std::optional<size_t> default_value_begin = [&] () -> std::optional<size_t> {
        if (!prop.default_value) {
          return std::nullopt;
        }
        sv_cache.emplace_back(toStringView(*prop.default_value));
        return sv_cache.size() - 1;
      }();
      properties.push_back(MinifiProperty{
        .name = toStringView(prop.name),
        .display_name = toStringView(prop.display_name),
        .description = toStringView(prop.description),
        .is_required = prop.is_required,
        .is_sensitive = prop.is_sensitive,

        .default_value = default_value_begin ? sv_cache.data() + default_value_begin.value() : nullptr,
        .allowed_values_count = prop.allowed_values.size(),
        .allowed_values_ptr = sv_cache.data() + allowed_values_begin,
        .validator = MinifiGetStandardValidator(toStandardPropertyValidator(prop.validator)),

        .type = allowed_types_begin ? sv_cache.data() + allowed_types_begin.value() : nullptr,
        .supports_expression_language = prop.supports_expression_language
      });
      cache.emplace_back(std::move(sv_cache));
    }
  return properties;
}

std::error_code make_error_code(MinifiStatus status);

}  // namespace org::apache::nifi::minifi::api::utils
