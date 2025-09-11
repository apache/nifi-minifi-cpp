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

#include "core/ConfigurableComponentImpl.h"

#include "minifi-cpp/core/Property.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::core {
void ConfigurableComponentImpl::setSupportedProperties(std::span<const core::PropertyReference> properties) {
  if (!canEdit()) { return; }

  std::lock_guard<std::mutex> lock(configuration_mutex_);

  supported_properties_.clear();
  for (const auto& item: properties) { supported_properties_.emplace(item.name, item); }
}

void ConfigurableComponentImpl::setSupportedProperties(std::span<const Property> properties) {
  if (!canEdit()) { return; }

  std::lock_guard<std::mutex> lock(configuration_mutex_);
  supported_properties_.clear();
  for (const auto& item: properties) { supported_properties_.emplace(item.getName(), item); }
}


nonstd::expected<std::string, std::error_code> ConfigurableComponentImpl::getProperty(const std::string_view name) const {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  const auto it = supported_properties_.find(name);
  if (it == supported_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty); }
  const Property& prop = it->second;
  return prop.getValue() | utils::transform([](const std::string_view value_view) -> std::string { return std::string{value_view}; });
}

nonstd::expected<void, std::error_code> ConfigurableComponentImpl::setProperty(const std::string_view name, std::string value) {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  const auto it = supported_properties_.find(name);
  if (it == supported_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty); }
  Property& prop = it->second;

  return prop.setValue(std::move(value));
}

nonstd::expected<void, std::error_code> ConfigurableComponentImpl::clearProperty(const std::string_view name) {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  const auto it = supported_properties_.find(name);
  if (it == supported_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty); }
  Property& prop = it->second;

  prop.clearValues();
  return {};
}

nonstd::expected<void, std::error_code> ConfigurableComponentImpl::appendProperty(const std::string_view name, std::string value) {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  const auto it = supported_properties_.find(name);
  if (it == supported_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty); }
  Property& prop = it->second;

  return prop.appendValue(std::move(value));
}

nonstd::expected<std::string, std::error_code> ConfigurableComponentImpl::getDynamicProperty(const std::string_view name) const {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  if (!supportsDynamicProperties()) {
    return nonstd::make_unexpected(PropertyErrorCode::DynamicPropertiesNotSupported);
  }
  const auto it = dynamic_properties_.find(name);
  if (it == dynamic_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::PropertyNotSet); }
  const Property& prop = it->second;
  return prop.getValue() | utils::transform([](const std::string_view value_view) -> std::string { return std::string{value_view}; });
}

nonstd::expected<void, std::error_code> ConfigurableComponentImpl::setDynamicProperty(std::string name, std::string value) {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  if (!supportsDynamicProperties()) {
    return nonstd::make_unexpected(PropertyErrorCode::DynamicPropertiesNotSupported);
  }
  Property& prop = dynamic_properties_[std::move(name)];
  prop.setSupportsExpressionLanguage(true);  // all dynamic properties support EL
  return prop.setValue(std::move(value));
}

nonstd::expected<void, std::error_code> ConfigurableComponentImpl::appendDynamicProperty(const std::string_view name, std::string value) {
  const std::lock_guard<std::mutex> lock(configuration_mutex_);
  if (!supportsDynamicProperties()) {
    return nonstd::make_unexpected(PropertyErrorCode::DynamicPropertiesNotSupported);
  }
  Property& prop = dynamic_properties_[std::string{name}];
  prop.setSupportsExpressionLanguage(true);  // all dynamic properties support EL
  return prop.appendValue(std::move(value));
}

std::vector<std::string> ConfigurableComponentImpl::getDynamicPropertyKeys() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  return dynamic_properties_ | ranges::views::transform([](const auto& kv) { return kv.first; }) | ranges::to<std::vector>();
}

std::map<std::string, std::string> ConfigurableComponentImpl::getDynamicProperties() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  std::map<std::string, std::string> result;
  for (const auto& [key, value]: dynamic_properties_) {
    result[key] = value.getValue().value_or("");
  }
  return result;
}

[[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> ConfigurableComponentImpl::getAllPropertyValues(const std::string_view name) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  const auto it = supported_properties_.find(name);
  if (it == supported_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty); }
  const Property& prop = it->second;
  return prop.getAllValues() | utils::transform([](const auto& values) -> std::vector<std::string> { return std::vector<std::string>{values.begin(), values.end()}; });
}

[[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> ConfigurableComponentImpl::getAllDynamicPropertyValues(const std::string_view name) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  const auto it = dynamic_properties_.find(name);
  if (it == dynamic_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::PropertyNotSet); }
  const Property& prop = it->second;
  return prop.getAllValues() | utils::transform([](const auto& values) -> std::vector<std::string> { return std::vector<std::string>{values.begin(), values.end()}; });
}

[[nodiscard]] std::map<std::string, Property, std::less<>> ConfigurableComponentImpl::getSupportedProperties() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  std::map<std::string, Property, std::less<>> supported_properties;
  for (const auto& [name, prop]: supported_properties_) {
    supported_properties.emplace(name, prop);
  }
  return supported_properties;
}

[[nodiscard]] nonstd::expected<Property, std::error_code> ConfigurableComponentImpl::getSupportedProperty(const std::string_view name) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  const auto it = supported_properties_.find(name);
  if (it == supported_properties_.end()) { return nonstd::make_unexpected(PropertyErrorCode::NotSupportedProperty); }
  return it->second;
}

}  // namespace org::apache::nifi::minifi::core
