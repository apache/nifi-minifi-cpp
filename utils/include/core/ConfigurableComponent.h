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

#include "core/Core.h"
#include <mutex>
#include <iostream>
#include <map>
#include <memory>

#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/Property.h"
#include "utils/gsl.h"
#include "minifi-cpp/core/ConfigurableComponent.h"
#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::core {

/**
 * Represents a configurable component
 * Purpose: Extracts configuration items for all components and localized them
 */
class ConfigurableComponentImpl : public virtual ConfigurableComponent {
 public:
  ConfigurableComponentImpl();

  ConfigurableComponentImpl(const ConfigurableComponentImpl &other) = delete;
  ConfigurableComponentImpl(ConfigurableComponentImpl &&other) = delete;

  ConfigurableComponentImpl& operator=(const ConfigurableComponentImpl &other) = delete;
  ConfigurableComponentImpl& operator=(ConfigurableComponentImpl &&other) = delete;

  using ConfigurableComponent::getProperty;

  bool getProperty(const std::string& name, uint64_t& value) const override {return getPropertyImpl(name, value);}
  bool getProperty(const std::string& name, int64_t& value) const override {return getPropertyImpl(name, value);}
  bool getProperty(const std::string& name, uint32_t& value) const override {return getPropertyImpl(name, value);}
  bool getProperty(const std::string& name, int& value) const override {return getPropertyImpl(name, value);}
  bool getProperty(const std::string& name, bool& value) const override {return getPropertyImpl(name, value);}
  bool getProperty(const std::string& name, double& value) const override {return getPropertyImpl(name, value);}
  bool getProperty(const std::string& name, std::string& value) const override {return getPropertyImpl(name, value);}

  /**
   * Provides a reference for the property.
   */
  bool getProperty(const std::string &name, Property &prop) const override;
  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return result of setting property.
   */
  bool setProperty(const std::string& name, const std::string& value) override;

  /**
   * Updates the Property from the key (name), adding value
   * to the collection of values within the Property.
   */
  bool updateProperty(const std::string &name, const std::string &value) override;

  bool updateProperty(const PropertyReference& property, std::string_view value) override;

  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return whether property was set or not
   */
  bool setProperty(const Property& prop, const std::string& value) override;

  bool setProperty(const PropertyReference& property, std::string_view value) override;

  /**
     * Sets the property using the provided name
     * @param property name
     * @param value property value.
     * @return whether property was set or not
     */
  bool setProperty(const Property& prop, PropertyValue &value) override;

  void setSupportedProperties(std::span<const core::PropertyReference> properties);

  /**
   * Gets whether or not this processor supports dynamic properties.
   *
   * @return true if this component supports dynamic properties (default is false)
   */
  bool supportsDynamicProperties() const override = 0;

  /**
   * Gets whether or not this processor supports dynamic relationships.
   *
   * @return true if this component supports dynamic relationships (default is false)
   */
  bool supportsDynamicRelationships() const override = 0;

  /**
   * Gets the value of a dynamic property (if it was set).
   *
   * @param name
   * @param value
   * @return
   */
  bool getDynamicProperty(const std::string& name, std::string &value) const override;

  bool getDynamicProperty(const std::string& name, core::Property &item) const override;

  /**
   * Sets the value of a new dynamic property.
   *
   * @param name
   * @param value
   * @return
   */
  bool setDynamicProperty(const std::string& name, const std::string& value) override;

  /**
   * Updates the value of an existing dynamic property.
   *
   * @param name
   * @param value
   * @return
   */
  bool updateDynamicProperty(const std::string &name, const std::string &value) override;

  /**
   * Invoked anytime a static property is modified
   */
  void onPropertyModified(const Property& /*old_property*/, const Property& /*new_property*/) override {
  }

  /**
   * Invoked anytime a dynamic property is modified.
   */
  void onDynamicPropertyModified(const Property& /*old_property*/, const Property& /*new_property*/) override {
  }

  /**
   * Provides all dynamic property keys that have been set.
   *
   * @return vector of property keys
   */
  std::vector<std::string> getDynamicPropertyKeys() const override;

  /**
   * Returns a vector all properties
   *
   * @return map of property keys to Property instances.
   */
  std::map<std::string, Property> getProperties() const override;

  /**
   * @return if property exists and is explicitly set, not just falling back to default value
   */
  bool isPropertyExplicitlySet(const Property&) const override;
  bool isPropertyExplicitlySet(const PropertyReference&) const override;

  ~ConfigurableComponentImpl() override;

  void initialize() override {
  }

 protected:
  void setAcceptAllProperties() {
    accept_all_properties_ = true;
  }

  /**
   * Returns true if the instance can be edited.
   * @return true/false
   */
  bool canEdit() override = 0;

  mutable std::mutex configuration_mutex_;

  bool accept_all_properties_;

  // Supported properties
  std::map<std::string, Property> properties_;

  // Dynamic properties
  std::map<std::string, Property> dynamic_properties_;

  virtual const Property* findProperty(const std::string& name) const;

 private:
  template<typename T>
  bool getPropertyImpl(const std::string& name, T &value) const;

  Property* findProperty(const std::string& name);
  std::shared_ptr<logging::Logger> logger_;

  bool createDynamicProperty(const std::string &name, const std::string &value);
};

template<typename T>
bool ConfigurableComponentImpl::getPropertyImpl(const std::string& name, T &value) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  const auto prop_ptr = findProperty(name);
  if (prop_ptr != nullptr) {
    const Property& property = *prop_ptr;
    if (property.getValue().getValue() == nullptr) {
      // empty value
      if (property.getRequired()) {
        logger_->log_error("Component {} required property {} is empty", name, property.getName());
        throw utils::internal::RequiredPropertyMissingException("Required property is empty: " + property.getName());
      }
      logger_->log_debug("Component {} property name {}, empty value", name, property.getName());
      return false;
    }
    logger_->log_debug("Component {} property name {} value {}", name, property.getName(), property.getValue().to_string());

    value = static_cast<T>(property.getValue());  // cast throws if the value is invalid
    return true;
  } else {
    logger_->log_warn("Could not find property {}", name);
    return false;
  }
}

template<typename T>
bool ConfigurableComponent::getProperty(const std::string& name, T& value) const {
  if constexpr (std::is_base_of<TransformableValue, T>::value) {
    std::string prop_str;
    if (!getProperty(name, prop_str)) {
      return false;
    }
    value = T(std::move(prop_str));
    return true;
  } else {
    return getProperty(name, value);
  }
}

template<typename T>
bool ConfigurableComponent::getProperty(const std::string& name, std::optional<T>& value) const {
  T raw_value;
  if (getProperty(name, raw_value)) {
    value = raw_value;
    return true;
  }
  return false;
}

template<typename T>
bool ConfigurableComponent::getProperty(const core::PropertyReference& property, T& value) const {
  return getProperty(std::string{property.name}, value);
}

template<typename T> requires(std::is_default_constructible_v<T>)
std::optional<T> ConfigurableComponent::getProperty(const std::string& property_name) const {
  T value;
  try {
    if (!getProperty(property_name, value)) return std::nullopt;
  } catch (const utils::internal::ValueException&) {
    return std::nullopt;
  }
  return value;
}

template<typename T> requires(std::is_default_constructible_v<T>)
std::optional<T> ConfigurableComponent::getProperty(const core::PropertyReference& property) const {
  return getProperty<T>(std::string(property.name));
}

}  // namespace org::apache::nifi::minifi::core
