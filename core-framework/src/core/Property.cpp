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

#include "core/PropertyDefinition.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/PropertyErrors.h"
#include "range/v3/algorithm/none_of.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"

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

void Property::setSupportsExpressionLanguage(bool supportEl) {
  supports_el_ = supportEl;
}

bool Property::operator<(const Property& right) const {
  return name_ < right.name_;
}

std::vector<std::string> Property::getDependentProperties() const {
  return dependent_properties_;
}

std::vector<std::pair<std::string, std::string>> Property::getExclusiveOfProperties() const {
  return exclusive_of_properties_;
}

namespace {
inline std::vector<std::string> createStrings(std::span<const std::string_view> string_views) {
  return ranges::views::transform(string_views, [](const auto& string_view) { return std::string{string_view}; }) | ranges::to<std::vector>;
}

inline std::vector<std::pair<std::string, std::string>> createStrings(std::span<const std::pair<std::string_view, std::string_view>> pairs_of_string_views) {
  return ranges::views::transform(pairs_of_string_views, [](const auto& pair_of_string_views) { return std::pair<std::string, std::string>(pair_of_string_views); }) | ranges::to<std::vector>;
}
}  // namespace

Property::Property(const PropertyReference& compile_time_property)
    : name_(compile_time_property.name),
      display_name_(compile_time_property.display_name),
      description_(compile_time_property.description),
      is_required_(compile_time_property.is_required),
      is_sensitive_(compile_time_property.is_sensitive),
      dependent_properties_(createStrings(compile_time_property.dependent_properties)),
      exclusive_of_properties_(createStrings(compile_time_property.exclusive_of_properties)),
      is_collection_(false),
      default_value_(compile_time_property.default_value),
      allowed_values_(createStrings(compile_time_property.allowed_values)),
      validator_(compile_time_property.validator),
      types_(createStrings(compile_time_property.allowed_types)),
      supports_el_(compile_time_property.supports_expression_language),
      is_transient_(false) {}

Property::Property(std::string name, std::string description, const std::string& value, bool is_required, std::vector<std::string> dependent_properties,
    std::vector<std::pair<std::string, std::string>> exclusive_of_properties)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(is_required),
      dependent_properties_(std::move(dependent_properties)),
      exclusive_of_properties_(std::move(exclusive_of_properties)),
      is_collection_(false),
      default_value_(value),
      validator_(&StandardPropertyValidators::ALWAYS_VALID_VALIDATOR),
      supports_el_(false),
      is_transient_(false) {}

Property::Property(std::string name, std::string description, const std::string& value)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(false),
      is_collection_(false),
      default_value_(value),
      validator_{&StandardPropertyValidators::ALWAYS_VALID_VALIDATOR},
      supports_el_(false),
      is_transient_(false) {}

Property::Property(std::string name, std::string description)
    : name_(std::move(name)),
      description_(std::move(description)),
      is_required_(false),
      is_collection_(true),
      validator_{&StandardPropertyValidators::ALWAYS_VALID_VALIDATOR},
      supports_el_(false),
      is_transient_(false) {}

Property::Property() : is_required_(false), is_collection_(false), validator_{&StandardPropertyValidators::ALWAYS_VALID_VALIDATOR}, supports_el_(false), is_transient_(false) {}

nonstd::expected<std::string_view, std::error_code> Property::getValue() const {
  if (!values_.empty()) { return values_.back(); }
  if (default_value_) { return *default_value_; }
  return nonstd::make_unexpected(PropertyErrorCode::PropertyNotSet);
}

nonstd::expected<std::span<const std::string>, std::error_code> Property::getAllValues() const {
  return std::span{values_};
}

nonstd::expected<void, std::error_code> Property::setValue(std::string value) {
  if (!validator_->validate(value)) { return nonstd::make_unexpected(PropertyErrorCode::ValidationFailed); }
  if (!allowed_values_.empty() && ranges::none_of(allowed_values_, [&](const auto& allowed_value) -> bool { return utils::string::toLower(allowed_value) == utils::string::toLower(value); })) {
    return nonstd::make_unexpected(PropertyErrorCode::ValidationFailed);
  }
  values_.clear();
  values_.push_back(std::move(value));
  return {};
}

nonstd::expected<void, std::error_code> Property::appendValue(std::string value) {
  if (!validator_->validate(value)) { return nonstd::make_unexpected(PropertyErrorCode::ValidationFailed); }
  if (!allowed_values_.empty() && ranges::none_of(allowed_values_, [&](const auto& allowed_value) -> bool { return utils::string::toLower(allowed_value) == utils::string::toLower(value); })) {
    return nonstd::make_unexpected(PropertyErrorCode::ValidationFailed);
  }
  if (values_.empty() && default_value_) { values_.push_back(*default_value_); }
  values_.push_back(std::move(value));
  return {};
}

void Property::clearValues() {
  values_.clear();
}

}  // namespace org::apache::nifi::minifi::core
