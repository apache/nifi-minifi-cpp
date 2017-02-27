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

#include "core/ConfigurableComponent.h"

#include "core/Property.h"
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

ConfigurableComponent::ConfigurableComponent(std::shared_ptr<logging::Logger> logger)
    : logger_(logger) {

}

ConfigurableComponent::ConfigurableComponent(
    const ConfigurableComponent &&other)
    : properties_(std::move(other.properties_)),
      logger_(std::move(other.logger_)) {

}
ConfigurableComponent::~ConfigurableComponent() {

}

/**
 * Get property using the provided name.
 * @param name property name.
 * @param value value passed in by reference
 * @return result of getting property.
 */
bool ConfigurableComponent::getProperty(const std::string name,
                                        std::string &value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);

  auto &&it = properties_.find(name);

  if (it != properties_.end()) {
    Property item = it->second;
    value = item.getValue();
    logger_->log_info("Processor %s property name %s value %s", name.c_str(),
                      item.getName().c_str(), value.c_str());
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
bool ConfigurableComponent::setProperty(const std::string name,
                                        std::string value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto &&it = properties_.find(name);

  if (it != properties_.end()) {
    Property item = it->second;
    item.setValue(value);
    properties_[item.getName()] = item;
    logger_->log_info("Component %s property name %s value %s", name.c_str(),
                      item.getName().c_str(), value.c_str());
    return true;
  } else {
    return false;
  }
}

/**
 * Sets the property using the provided name
 * @param property name
 * @param value property value.
 * @return whether property was set or not
 */
bool ConfigurableComponent::setProperty(Property &prop, std::string value) {
  std::lock_guard<std::mutex> lock(configuration_mutex_);
  auto it = properties_.find(prop.getName());

  if (it != properties_.end()) {
    Property item = it->second;
    item.setValue(value);
    properties_[item.getName()] = item;
    logger_->log_info("property name %s value %s", prop.getName().c_str(),
                      item.getName().c_str(), value.c_str());
    return true;
  } else {
    Property newProp(prop);
    newProp.setValue(value);
    properties_.insert(
        std::pair<std::string, Property>(prop.getName(), newProp));
    return true;

  }
  return false;
}

/**
 * Sets supported properties for the ConfigurableComponent
 * @param supported properties
 * @return result of set operation.
 */
bool ConfigurableComponent::setSupportedProperties(
    std::set<Property> properties) {
  if (!canEdit()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(configuration_mutex_);

  properties_.clear();
  for (auto item : properties) {
    properties_[item.getName()] = item;
  }

  return true;
}

} /* namespace components */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
