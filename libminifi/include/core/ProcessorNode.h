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

#ifndef LIBMINIFI_INCLUDE_PROCESSOR_PROCESSORNODE_H_
#define LIBMINIFI_INCLUDE_PROCESSOR_PROCESSORNODE_H_

#include "ConfigurableComponent.h"
#include "Connectable.h"
#include "Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Processor node functions as a pass through to the implementing Connectables
 */
class ProcessorNode : public ConfigurableComponent, public Connectable {
 public:
  explicit ProcessorNode(const std::shared_ptr<Connectable> &processor);

  explicit ProcessorNode(const ProcessorNode &other);

  explicit ProcessorNode(const ProcessorNode &&other);

  /**
   * Get property using the provided name.
   * @param name property name.
   * @param value value passed in by reference
   * @return result of getting property.
   */
  std::shared_ptr<Connectable> getProcessor() const {
    return processor_;
  }

  void yield() {
    processor_->yield();
  }

  /**
   * Get property using the provided name.
   * @param name property name.
   * @param value value passed in by reference
   * @return result of getting property.
   */
  template<typename T>
  bool getProperty(const std::string &name, T &value) {
    const std::shared_ptr<ConfigurableComponent> processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    if (nullptr != processor_cast)
      return processor_cast->getProperty<T>(name, value);
    else {
      return ConfigurableComponent::getProperty<T>(name, value);
    }
  }
  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return result of setting property.
   */
  bool setProperty(const std::string &name, std::string value) {
    const std::shared_ptr<ConfigurableComponent> processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    bool ret = ConfigurableComponent::setProperty(name, value);
    if (nullptr != processor_cast)
      ret = processor_cast->setProperty(name, value);

    return ret;

  }

  /**
   * Get dynamic property using the provided name.
   * @param name property name.
   * @param value value passed in by reference
   * @return result of getting property.
   */
  bool getDynamicProperty(const std::string name, std::string &value) const {
    const auto &processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    if (processor_cast) {
      return processor_cast->getDynamicProperty(name, value);
    } else {
      return ConfigurableComponent::getDynamicProperty(name, value);
    }
  }

  /**
   * Returns theflow version
   * @returns flow version. can be null if a flow version is not tracked.
   */
  virtual std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const {
    if (processor_ != nullptr) {
      return processor_->getFlowIdentifier();
    } else {
      return connectable_version_;
    }
  }

  /**
   * Sets the dynamic property using the provided name
   * @param property name
   * @param value property value.
   * @return result of setting property.
   */
  bool setDynamicProperty(const std::string name, std::string value) {
    const auto &processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    auto ret = ConfigurableComponent::setDynamicProperty(name, value);

    if (processor_cast) {
      ret = processor_cast->setDynamicProperty(name, value);
    }

    return ret;
  }

  /**
   * Gets list of dynamic property keys
   * @param name property name.
   * @param value value passed in by reference
   * @return result of getting property.
   */
  std::vector<std::string> getDynamicPropertyKeys() const {
    const auto &processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    if (processor_cast) {
      return processor_cast->getDynamicPropertyKeys();
    } else {
      return ConfigurableComponent::getDynamicPropertyKeys();
    }
  }

  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return whether property was set or not
   */
  bool setProperty(Property &prop, std::string value) {
    const std::shared_ptr<ConfigurableComponent> processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    bool ret = ConfigurableComponent::setProperty(prop, value);
    if (nullptr != processor_cast)
      ret = processor_cast->setProperty(prop, value);

    return ret;
  }

  /**
   * Sets supported properties for the ConfigurableComponent
   * @param supported properties
   * @return result of set operation.
   */
  bool setSupportedProperties(std::set<Property> properties) {
    const std::shared_ptr<ConfigurableComponent> processor_cast = std::dynamic_pointer_cast<ConfigurableComponent>(processor_);
    bool ret = ConfigurableComponent::setSupportedProperties(properties);
    if (nullptr != processor_cast)
      ret = processor_cast->setSupportedProperties(properties);

    return ret;
  }
  /**
   * Sets supported properties for the ConfigurableComponent
   * @param supported properties
   * @return result of set operation.
   */

  bool setAutoTerminatedRelationships(std::set<Relationship> relationships) {
    return processor_->setAutoTerminatedRelationships(relationships);
  }

  bool isAutoTerminated(Relationship relationship) {
    return processor_->isAutoTerminated(relationship);
  }

  bool setSupportedRelationships(std::set<Relationship> relationships) {
    return processor_->setSupportedRelationships(relationships);
  }

  bool isSupportedRelationship(Relationship relationship) {
    return processor_->isSupportedRelationship(relationship);
  }

  /**
   * Set name.
   * @param name
   */
  void setName(const std::string &name) {
    Connectable::setName(name);
    processor_->setName(name);
  }

  /**
   * Set UUID in this instance
   * @param uuid uuid to apply to the internal representation.
   */
  void setUUID(utils::Identifier &uuid) {
    Connectable::setUUID(uuid);
    processor_->setUUID(uuid);
  }

// Get Processor penalization period in MilliSecond
  uint64_t getPenalizationPeriodMsec(void) {
    return processor_->getPenalizationPeriodMsec();
  }

  /**
   * Get outgoing connection based on relationship
   * @return set of outgoing connections.
   */
  std::set<std::shared_ptr<Connectable>> getOutGoingConnections(std::string relationship) {
    return processor_->getOutGoingConnections(relationship);
  }

  /**
   * Get next incoming connection
   * @return next incoming connection
   */
  std::shared_ptr<Connectable> getNextIncomingConnection() {
    return processor_->getNextIncomingConnection();
  }

  /**
   * @return true if incoming connections > 0
   */
  bool hasIncomingConnections() {
    return processor_->hasIncomingConnections();
  }

  /**
   * Returns the UUID through the provided object.
   * @param uuid uuid struct to which we will copy the memory
   * @return success of request
   */
  bool getUUID(utils::Identifier &uuid) {
    return processor_->getUUID(uuid);
  }
  /*

   unsigned const char *getUUID() {
   return processor_->getUUID();
   }*/
  /**
   * Return the UUID string
   * @param constant reference to the UUID str
   */
  const std::string & getUUIDStr() const {
    return processor_->getUUIDStr();
  }

// Get Process Name
  std::string getName() const {
    return processor_->getName();
  }

  uint8_t getMaxConcurrentTasks() {
    return processor_->getMaxConcurrentTasks();
  }

  void setMaxConcurrentTasks(const uint8_t tasks) {
    processor_->setMaxConcurrentTasks(tasks);
  }

  virtual bool supportsDynamicProperties() {
    return false;
  }

  virtual bool isRunning();

  virtual bool isWorkAvailable();

  virtual ~ProcessorNode();

 protected:

  virtual bool canEdit() {
    return !processor_->isRunning();
  }

  /**
   * internal connectable.
   */
  std::shared_ptr<Connectable> processor_;

}
;

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_PROCESSOR_PROCESSORNODE_H_ */
