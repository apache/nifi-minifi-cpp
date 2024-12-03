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
#include <vector>

#include "Core.h"
#include <mutex>
#include <iostream>
#include <map>
#include <memory>

#include "logging/Logger.h"
#include "Property.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

/**
 * Represents a configurable component
 * Purpose: Extracts configuration items for all components and localized them
 */
class ConfigurableComponent {
 public:
  virtual bool getProperty(const std::string& name, uint64_t& value) const = 0;
  virtual bool getProperty(const std::string& name, int64_t& value) const = 0;
  virtual bool getProperty(const std::string& name, uint32_t& value) const = 0;
  virtual bool getProperty(const std::string& name, int& value) const = 0;
  virtual bool getProperty(const std::string& name, bool& value) const = 0;
  virtual bool getProperty(const std::string& name, double& value) const = 0;
  virtual bool getProperty(const std::string& name, std::string& value) const = 0;


  virtual bool getProperty(const std::string &name, Property &prop) const = 0;
  virtual bool setProperty(const std::string& name, const std::string& value) = 0;
  virtual bool updateProperty(const std::string &name, const std::string &value) = 0;
  virtual bool updateProperty(const PropertyReference& property, std::string_view value) = 0;
  virtual bool setProperty(const Property& prop, const std::string& value) = 0;
  virtual bool setProperty(const PropertyReference& property, std::string_view value) = 0;
  virtual bool setProperty(const Property& prop, PropertyValue &value) = 0;
  virtual bool supportsDynamicProperties() const = 0;
  virtual bool supportsDynamicRelationships() const = 0;
  virtual bool getDynamicProperty(const std::string& name, std::string &value) const = 0;
  virtual bool getDynamicProperty(const std::string& name, core::Property &item) const = 0;
  virtual bool setDynamicProperty(const std::string& name, const std::string& value) = 0;
  virtual bool updateDynamicProperty(const std::string &name, const std::string &value) = 0;
  virtual void onPropertyModified(const Property& /*old_property*/, const Property& /*new_property*/) = 0;
  virtual void onDynamicPropertyModified(const Property& /*old_property*/, const Property& /*new_property*/) = 0;
  virtual std::vector<std::string> getDynamicPropertyKeys() const = 0;
  virtual std::map<std::string, Property> getProperties() const = 0;
  virtual bool isPropertyExplicitlySet(const Property&) const = 0;
  virtual bool isPropertyExplicitlySet(const PropertyReference&) const = 0;
  virtual ~ConfigurableComponent() = default;
  virtual void initialize() = 0;
  virtual bool canEdit() = 0;

  template<typename T>
  bool getProperty(const std::string& name, T& value) const;

  template<typename T>
  bool getProperty(const std::string& name, std::optional<T>& value) const;

  template<typename T>
  bool getProperty(const core::PropertyReference& property, T& value) const;

  template<typename T = std::string> requires(std::is_default_constructible_v<T>)
  std::optional<T> getProperty(const std::string& property_name) const;

  template<typename T = std::string> requires(std::is_default_constructible_v<T>)
  std::optional<T> getProperty(const core::PropertyReference& property) const;
};

}  // namespace org::apache::nifi::minifi::core
