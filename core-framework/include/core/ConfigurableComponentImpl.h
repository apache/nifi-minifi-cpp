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

#include <span>

#include "minifi-cpp/core/ConfigurableComponent.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

#include "minifi-cpp/core/Property.h"

namespace org::apache::nifi::minifi::core {


class ConfigurableComponentImpl : public virtual ConfigurableComponent {
 public:
  [[nodiscard]] nonstd::expected<std::string, std::error_code> getProperty(std::string_view name) const override;
  nonstd::expected<void, std::error_code> setProperty(std::string_view name, std::string value) override;
  nonstd::expected<void, std::error_code> clearProperty(std::string_view name) override;

  [[nodiscard]] nonstd::expected<std::string, std::error_code> getDynamicProperty(std::string_view name) const override;
  nonstd::expected<void, std::error_code> setDynamicProperty(std::string name, std::string value) override;

  void setSupportedProperties(std::span<const PropertyReference> properties);
  void setSupportedProperties(std::span<const Property> properties);

  [[nodiscard]] std::map<std::string, Property, std::less<>> getSupportedProperties() const override;
  [[nodiscard]] nonstd::expected<Property, std::error_code> getSupportedProperty(std::string_view name) const override;
  [[nodiscard]] std::vector<std::string> getDynamicPropertyKeys() const override;

  [[nodiscard]] std::map<std::string, std::string> getDynamicProperties() const override;


  // for property sequences
  nonstd::expected<void, std::error_code> appendProperty(std::string_view name, std::string value) override;
  nonstd::expected<void, std::error_code> appendDynamicProperty(std::string_view name, std::string value) override;
  [[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const override;
  [[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> getAllDynamicPropertyValues(std::string_view name) const override;

 private:
  std::shared_ptr<logging::Logger> logger_ = logging::LoggerFactory<ConfigurableComponentImpl>::getLogger();

  mutable std::mutex configuration_mutex_;

  // std::less<> for heterogeneous lookup
  std::map<std::string, Property, std::less<>> supported_properties_;
  std::map<std::string, Property, std::less<>> dynamic_properties_;
};


}  // namespace org::apache::nifi::minifi::core
