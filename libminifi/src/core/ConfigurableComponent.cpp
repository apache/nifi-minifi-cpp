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

#include <utility>
#include <string>
#include <map>
#include <vector>

#include "core/ConfigurableComponent.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {

ConfigurableComponent::ConfigurableComponent()
    : accept_all_properties_(false),
      logger_(logging::LoggerFactory<ConfigurableComponent>::getLogger()) {
}

ConfigurableComponent::~ConfigurableComponent() = default;

bool ConfigurableComponent::getProperty(const std::string &name, Property &prop) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  auto &&it = properties_.find(name);

  if (it != properties_.end()) {
    prop = it->second;
    return true;
  } else {
    return false;
  }
}

/**
 * Sets the property using the provided name
 * @param property name
 * @param value property value.
 * @return result of setting property.
 */
bool ConfigurableComponent::setProperty(const std::string& name, const std::string& value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto it = properties_.find(name);

  if (it != properties_.end()) {
    Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&]{
      onPropertyModified(orig_property, new_property);
      logger_->log_debug("Component %s property name %s value %s", name, new_property.getName(), value);
    });
    new_property.setValue(value);
    return true;
  } else {
    if (accept_all_properties_) {
      static const std::string STAR_PROPERTIES = "Property";
      Property new_property(name, STAR_PROPERTIES, value, false, "", { }, { });
      new_property.setTransient();
      new_property.setValue(value);
      properties_.insert(std::pair<std::string, Property>(name, new_property));
      return true;
    } else {
      logger_->log_debug("Component %s cannot be set to %s", name, value);
      return false;
    }
  }
}

/**
 * Sets the property using the provided name
 * @param property name
 * @param value property value.
 * @return result of setting property.
 */
bool ConfigurableComponent::updateProperty(const std::string &name, const std::string &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto &&it = properties_.find(name);

  if (it != properties_.end()) {
    Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onPropertyModified(orig_property, new_property);
      logger_->log_debug("Component %s property name %s value %s", name, new_property.getName(), value);
    });
    new_property.addValue(value);
    return true;
  } else {
    return false;
  }
}

bool ConfigurableComponent::updateProperty(const PropertyReference& property, std::string_view value) {
  return updateProperty(std::string{property.name}, std::string{value});
}

/**
 * Sets the property using the provided name
 * @param property name
 * @param value property value.
 * @return whether property was set or not
 */
bool ConfigurableComponent::setProperty(const Property& prop, const std::string& value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto it = properties_.find(prop.getName());

  if (it != properties_.end()) {
    Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onPropertyModified(orig_property, new_property);
      logger_->log_debug("property name %s value %s and new value is %s", prop.getName(), value, new_property.getValue().to_string());
    });
    new_property.setValue(value);
    return true;
  } else {
    if (accept_all_properties_) {
      Property new_property(prop);
      new_property.setTransient();
      new_property.setValue(value);
      properties_.insert(std::pair<std::string, Property>(prop.getName(), new_property));
      logger_->log_debug("Adding transient property name %s value %s and new value is %s", prop.getName(), value, new_property.getValue().to_string());
      return true;
    } else {
      // Should not attempt to update dynamic properties here since the return code
      // is relied upon by other classes to determine if the property exists.
      return false;
    }
  }
}

bool ConfigurableComponent::setProperty(const PropertyReference& property, std::string_view value) {
  return setProperty(std::string{property.name}, std::string{value});
}

bool ConfigurableComponent::setProperty(const Property& prop, PropertyValue &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto it = properties_.find(prop.getName());

  if (it != properties_.end()) {
    Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onPropertyModified(orig_property, new_property);
      logger_->log_debug("property name %s value %s and new value is %s", prop.getName(), new_property.getName(), value, new_property.getValue().to_string());
    });
    new_property.setValue(value);
    return true;
  } else {
    if (accept_all_properties_) {
      Property new_property(prop);
      new_property.setTransient();
      new_property.setValue(value);
      properties_.insert(std::pair<std::string, Property>(prop.getName(), new_property));
      logger_->log_debug("Adding transient property name %s value %s and new value is %s", prop.getName(), value, new_property.getValue().to_string());
      return true;
    } else {
      // Should not attempt to update dynamic properties here since the return code
      // is relied upon by other classes to determine if the property exists.
      return false;
    }
  }
}

void ConfigurableComponent::setSupportedProperties(std::span<const core::PropertyReference> properties) {
  if (!canEdit()) {
    return;
  }

  std::lock_guard<std::mutex> lock(configuration_mutex_);

  properties_.clear();
  for (const auto& item : properties) {
    properties_.emplace(item.name, item);
  }
}

bool ConfigurableComponent::getDynamicProperty(const std::string& name, std::string &value) const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  auto &&it = dynamic_properties_.find(name);
  if (it != dynamic_properties_.end()) {
    const Property& item = it->second;
    if (item.getValue().getValue() == nullptr) {
      // empty property value
      if (item.getRequired()) {
        logger_->log_error("Component %s required dynamic property %s is empty", name, item.getName());
        throw std::runtime_error("Required dynamic property is empty: " + item.getName());
      }
      logger_->log_debug("Component %s dynamic property name %s, empty value", name, item.getName());
      return false;
    }
    value = item.getValue().to_string();
    logger_->log_debug("Component %s dynamic property name %s value %s", name, item.getName(), value);
    return true;
  } else {
    return false;
  }
}

bool ConfigurableComponent::createDynamicProperty(const std::string &name, const std::string &value) {
  if (!supportsDynamicProperties()) {
    logger_->log_debug("Attempted to create dynamic property %s, but this component does not support creation."
                       "of dynamic properties.",
                       name);
    return false;
  }

  static const std::string DEFAULT_DYNAMIC_PROPERTY_DESC = "Dynamic Property";
  Property new_property(name, DEFAULT_DYNAMIC_PROPERTY_DESC, value, false, "", { }, { });
  new_property.setSupportsExpressionLanguage(true);
  logger_->log_info("Processor %s dynamic property '%s' value '%s'", name.c_str(), new_property.getName().c_str(), value.c_str());
  dynamic_properties_[new_property.getName()] = new_property;
  onDynamicPropertyModified({ }, new_property);
  return true;
}

bool ConfigurableComponent::setDynamicProperty(const std::string& name, const std::string& value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto &&it = dynamic_properties_.find(name);

  if (it != dynamic_properties_.end()) {
    Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onDynamicPropertyModified(orig_property, new_property);
      logger_->log_debug("Component %s dynamic property name %s value %s", name, new_property.getName(), value);
    });
    new_property.setValue(value);
    new_property.setSupportsExpressionLanguage(true);
    return true;
  } else {
    return createDynamicProperty(name, value);
  }
}

bool ConfigurableComponent::updateDynamicProperty(const std::string &name, const std::string &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto &&it = dynamic_properties_.find(name);

  if (it != dynamic_properties_.end()) {
    Property orig_property = it->second;
    Property& new_property = it->second;
    auto onExit = gsl::finally([&] {
      onDynamicPropertyModified(orig_property, new_property);
      logger_->log_debug("Component %s dynamic property name %s value %s", name, new_property.getName(), value);
    });
    new_property.addValue(value);
    new_property.setSupportsExpressionLanguage(true);
    return true;
  } else {
    return createDynamicProperty(name, value);
  }
}

std::vector<std::string> ConfigurableComponent::getDynamicPropertyKeys() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  std::vector<std::string> result;

  for (const auto &pair : dynamic_properties_) {
    result.emplace_back(pair.first);
  }

  return result;
}

std::map<std::string, Property> ConfigurableComponent::getProperties() const {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  std::map<std::string, Property> result;

  for (const auto &pair : properties_) {
    result.insert({ pair.first, pair.second });
  }

  for (const auto &pair : dynamic_properties_) {
    result.insert({ pair.first, pair.second });
  }

  return result;
}

bool ConfigurableComponent::isPropertyExplicitlySet(const Property& searched_prop) const {
  Property prop;
  return getProperty(searched_prop.getName(), prop) && !prop.getValues().empty();
}

bool ConfigurableComponent::isPropertyExplicitlySet(const PropertyReference& searched_prop) const {
  Property prop;
  return getProperty(std::string(searched_prop.name), prop) && !prop.getValues().empty();
}

}  // namespace org::apache::nifi::minifi::core
