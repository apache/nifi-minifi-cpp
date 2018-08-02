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

#ifndef LIBMINIFI_INCLUDE_CORE_CONFIGURABLECOMPONENT_H_
#define LIBMINIFI_INCLUDE_CORE_CONFIGURABLECOMPONENT_H_

#include "Core.h"
#include <mutex>
#include <iostream>
#include <map>
#include <memory>
#include <set>

#define DEFAULT_DYNAMIC_PROPERTY_DESC "Dynamic Property"

#include "logging/Logger.h"
#include "Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Represents a configurable component
 * Purpose: Extracts configuration items for all components and localized them
 */
class ConfigurableComponent {
 public:

  ConfigurableComponent();

  explicit ConfigurableComponent(const ConfigurableComponent &&other);

  /**
   * Get property using the provided name.
   * @param name property name.
   * @param value value passed in by reference
   * @return result of getting property.
   */
  bool getProperty(const std::string name, std::string &value);

  /**
   * Provides a reference for the property.
   */
  bool getProperty(const std::string &name, Property &prop);
  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return result of setting property.
   */
  bool setProperty(const std::string name, std::string value);

  /**
   * Updates the Property from the key (name), adding value
   * to the collection of values within the Property.
   */
  bool updateProperty(const std::string &name, const std::string &value);
  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return whether property was set or not
   */
  bool setProperty(Property &prop, std::string value);

  /**
   * Sets supported properties for the ConfigurableComponent
   * @param supported properties
   * @return result of set operation.
   */
  bool setSupportedProperties(std::set<Property> properties);
  /**
   * Sets supported properties for the ConfigurableComponent
   * @param supported properties
   * @return result of set operation.
   */

  /**
   * Gets whether or not this processor supports dynamic properties.
   *
   * @return true if this component supports dynamic properties (default is false)
   */
  virtual bool supportsDynamicProperties() = 0;

  /**
   * Gets whether or not this processor supports dynamic relationships.
   *
   * @return true if this component supports dynamic relationships (default is false)
   */
  virtual bool supportsDynamicRelationships() {
    return false;
  }

  /**
   * Gets the value of a dynamic property (if it was set).
   *
   * @param name
   * @param value
   * @return
   */
  bool getDynamicProperty(const std::string name, std::string &value);

  /**
   * Sets the value of a new dynamic property.
   *
   * @param name
   * @param value
   * @return
   */
  bool setDynamicProperty(const std::string name, std::string value);

  /**
   * Updates the value of an existing dynamic property.
   *
   * @param name
   * @param value
   * @return
   */
  bool updateDynamicProperty(const std::string &name, const std::string &value);

  /**
   * Invoked anytime a static property is modified
   */
  virtual void onPropertyModified(const Property &old_property, const Property &new_property) {
  }

  /**
   * Invoked anytime a dynamic property is modified.
   */
  virtual void onDynamicPropertyModified(const Property &old_property, const Property &new_property) {
  }

  /**
   * Provides all dynamic property keys that have been set.
   *
   * @return vector of property keys
   */
  std::vector<std::string> getDynamicPropertyKeys();

  /**
   * Returns a vector all properties
   *
   * @return map of property keys to Property instances.
   */
  std::map<std::string, Property> getProperties();

  virtual ~ConfigurableComponent();

  virtual void initialize() {

  }

 protected:

  /**
   * Returns true if the instance can be edited.
   * @return true/false
   */
  virtual bool canEdit()= 0;

  std::mutex configuration_mutex_;

  // Supported properties
  std::map<std::string, Property> properties_;

  // Dynamic properties
  std::map<std::string, Property> dynamic_properties_;

 private:
  std::shared_ptr<logging::Logger> logger_;

  bool createDynamicProperty(const std::string &name, const std::string &value);

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CONFIGURABLECOMPONENT_H_ */
