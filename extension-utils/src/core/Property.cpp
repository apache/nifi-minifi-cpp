/**
 *
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

#include "core/Property.h"

#include <utility>

#include "core/PropertyType.h"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"
#include "core/PropertyDefinition.h"
#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::core {

std::string Property::getName() const {
  return name_;
}

std::string Property::getDisplayName() const {
  return display_name_.empty() ? name_ : display_name_;
}

std::string Property::getDescription() const {
  return description_;
}

std::vector<std::string> Property::getAllowedTypes() const {
  return types_;
}

const PropertyValue &Property::getValue() const {
  if (!values_.empty())
    return values_.front();
  else
    return default_value_;
}

bool Property::getRequired() const {
  return is_required_;
}

bool Property::isSensitive() const {
  return is_sensitive_;
}

bool Property::supportsExpressionLanguage() const {
  return supports_el_;
}

const PropertyValidator& Property::getValidator() const {
  return *validator_;
}

std::vector<std::string> Property::getValues() {
  return values_
         | ranges::views::transform([](const auto& property_value) { return property_value.to_string(); })
         | ranges::to<std::vector>();
}

void Property::setSupportsExpressionLanguage(bool supportEl) {
  supports_el_ = supportEl;
}

void Property::addValue(const std::string &value) {
  PropertyValue vn;
  vn = value;
  values_.push_back(std::move(vn));
}

bool Property::operator<(const Property &right) const {
  return name_ < right.name_;
}


std::vector<std::string> Property::getDependentProperties() const {
  return dependent_properties_;
}

std::vector<std::pair<std::string, std::string>> Property::getExclusiveOfProperties() const {
  return exclusive_of_properties_;
}

namespace {
std::vector<PropertyValue> createPropertyValues(std::span<const std::string_view> values, const core::PropertyParser& property_parser) {
  return ranges::views::transform(values, [&property_parser](const auto& value) {
    return property_parser.parse(value);
  }) | ranges::to<std::vector>;
}

inline std::vector<std::string> createStrings(std::span<const std::string_view> string_views) {
  return ranges::views::transform(string_views, [](const auto& string_view) { return std::string{string_view}; })
         | ranges::to<std::vector>;
}

inline std::vector<std::pair<std::string, std::string>> createStrings(std::span<const std::pair<std::string_view, std::string_view>> pairs_of_string_views) {
  return ranges::views::transform(pairs_of_string_views, [](const auto& pair_of_string_views) { return std::pair<std::string, std::string>(pair_of_string_views); })
         | ranges::to<std::vector>;
}
}  // namespace

Property::Property(const PropertyReference& compile_time_property)
    : name_(compile_time_property.name),
      description_(compile_time_property.description),
      is_required_(compile_time_property.is_required),
      is_sensitive_(compile_time_property.is_sensitive),
      dependent_properties_(createStrings(compile_time_property.dependent_properties)),
      exclusive_of_properties_(createStrings(compile_time_property.exclusive_of_properties)),
      is_collection_(false),
      validator_(compile_time_property.type),
      display_name_(compile_time_property.display_name),
      allowed_values_(createPropertyValues(compile_time_property.allowed_values, *compile_time_property.type)),
      types_(createStrings(compile_time_property.allowed_types)),
      supports_el_(compile_time_property.supports_expression_language),
      is_transient_(false) {
  if (compile_time_property.default_value) {
    default_value_ = compile_time_property.type->parse(*compile_time_property.default_value);
  }
}

Property::Property(std::string name, std::string description, const std::string& value, bool is_required, std::vector<std::string> dependent_properties,
         std::vector<std::pair<std::string, std::string>> exclusive_of_properties)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(is_required),
      dependent_properties_(std::move(dependent_properties)),
      exclusive_of_properties_(std::move(exclusive_of_properties)),
      is_collection_(false),
      validator_{&StandardPropertyTypes::VALID_TYPE},
      supports_el_(false),
      is_transient_(false) {
  default_value_ = coerceDefaultValue(value);
}

Property::Property(std::string name, std::string description, const std::string& value)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(false),
      is_collection_(false),
      validator_{&StandardPropertyTypes::VALID_TYPE},
      supports_el_(false),
      is_transient_(false) {
  default_value_ = coerceDefaultValue(value);
}

Property::Property(std::string name, std::string description)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(false),
      is_collection_(true),
      validator_{&StandardPropertyTypes::VALID_TYPE},
      supports_el_(false),
      is_transient_(false) {}

Property::Property()
    : is_required_(false),
      is_collection_(false),
      validator_{&StandardPropertyTypes::VALID_TYPE},
      supports_el_(false),
      is_transient_(false) {}

PropertyValue Property::coerceDefaultValue(const std::string &value) {
  PropertyValue ret;
  if (value == "false" || value == "true") {
    bool val;
    std::istringstream(value) >> std::boolalpha >> val;
    ret = val;
    validator_ = StandardPropertyTypes::getValidator(ret.getValue());
  } else {
    ret = value;
    validator_ = gsl::make_not_null(&StandardPropertyTypes::VALID_TYPE);
  }
  return ret;
}

template<typename T>
bool Property::StringToInt(std::string input, T &output) {
  return DataSizeValue::StringToInt<T>(input, output);
}

}  // namespace org::apache::nifi::minifi::core
