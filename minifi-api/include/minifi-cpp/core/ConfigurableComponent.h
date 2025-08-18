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

#include <string>
#include <string_view>
#include <vector>

#include "Core.h"
#include "Property.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::core {

class ConfigurableComponent : public virtual CoreComponent {
 public:
  virtual ~ConfigurableComponent() = default;

  virtual void initialize() {};
  virtual bool canEdit() = 0;

  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> getProperty(std::string_view name) const = 0;
  virtual nonstd::expected<void, std::error_code> setProperty(std::string_view name, std::string value) = 0;
  virtual nonstd::expected<void, std::error_code> appendProperty(std::string_view name, std::string value) = 0;
  virtual nonstd::expected<void, std::error_code> clearProperty(std::string_view name) = 0;

  [[nodiscard]] virtual nonstd::expected<std::string, std::error_code> getDynamicProperty(std::string_view name) const = 0;
  virtual nonstd::expected<void, std::error_code> setDynamicProperty(std::string name, std::string value) = 0;
  virtual nonstd::expected<void, std::error_code> appendDynamicProperty(std::string_view name, std::string value) = 0;

  [[nodiscard]] virtual std::vector<std::string> getDynamicPropertyKeys() const = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> getDynamicProperties() const = 0;

  [[nodiscard]] virtual nonstd::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const = 0;
  [[nodiscard]] virtual nonstd::expected<std::vector<std::string>, std::error_code> getAllDynamicPropertyValues(std::string_view name) const = 0;

  [[nodiscard]] virtual bool supportsDynamicProperties() const = 0;
  [[nodiscard]] virtual bool supportsDynamicRelationships() const = 0;

  [[nodiscard]] virtual std::map<std::string, Property, std::less<>> getSupportedProperties() const = 0;
  [[nodiscard]] virtual nonstd::expected<Property, std::error_code> getSupportedProperty(std::string_view name) const = 0;
};

}  // namespace org::apache::nifi::minifi::core
