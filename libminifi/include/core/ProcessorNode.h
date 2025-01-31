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

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/ConfigurableComponent.h"
#include "core/Connectable.h"
#include "core/Property.h"
#include "minifi-cpp/core/ProcessorNode.h"

namespace org::apache::nifi::minifi::core {

/**
 * Processor node functions as a pass through to the implementing Connectables
 */
class ProcessorNodeImpl : public ConfigurableComponentImpl, public ConnectableImpl, public virtual ProcessorNode {
 public:
  explicit ProcessorNodeImpl(Connectable* processor);

  ProcessorNodeImpl(const ProcessorNodeImpl &other) = delete;
  ProcessorNodeImpl(ProcessorNodeImpl &&other) = delete;

  ~ProcessorNodeImpl() override;

  ProcessorNodeImpl& operator=(const ProcessorNodeImpl &other) = delete;
  ProcessorNodeImpl& operator=(ProcessorNodeImpl &&other) = delete;

  /**
   * Get property using the provided name.
   * @param name property name.
   * @param value value passed in by reference
   * @return result of getting property.
   */
  Connectable* getProcessor() const override {
    return processor_;
  }

  void yield() override {
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
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(processor_);
    if (nullptr != processor_cast) {
      return processor_cast->getProperty<T>(name, value);
    } else {
      return ConfigurableComponentImpl::getProperty<T>(name, value);
    }
  }
  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return result of setting property.
   */
  bool setProperty(const std::string &name, const std::string& value) override {
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(processor_);
    bool ret = ConfigurableComponentImpl::setProperty(name, value);
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
  bool getDynamicProperty(const std::string& name, std::string &value) const override {
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(processor_);
    if (processor_cast) {
      return processor_cast->getDynamicProperty(name, value);
    } else {
      return ConfigurableComponentImpl::getDynamicProperty(name, value);
    }
  }

  /**
   * Returns theflow version
   * @returns flow version. can be null if a flow version is not tracked.
   */
  std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const override {
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
  bool setDynamicProperty(const std::string& name, const std::string& value) override {
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(processor_);
    auto ret = ConfigurableComponentImpl::setDynamicProperty(name, value);

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
  std::vector<std::string> getDynamicPropertyKeys() const override {
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(processor_);
    if (processor_cast) {
      return processor_cast->getDynamicPropertyKeys();
    } else {
      return ConfigurableComponentImpl::getDynamicPropertyKeys();
    }
  }

  /**
   * Sets the property using the provided name
   * @param property name
   * @param value property value.
   * @return whether property was set or not
   */
  bool setProperty(const Property &prop, const std::string& value) override {
    const auto processor_cast = dynamic_cast<ConfigurableComponent*>(processor_);
    bool ret = ConfigurableComponentImpl::setProperty(prop, value);
    if (nullptr != processor_cast)
      ret = processor_cast->setProperty(prop, value);

    return ret;
  }

  void setAutoTerminatedRelationships(std::span<const Relationship> relationships) override {
    processor_->setAutoTerminatedRelationships(relationships);
  }

  void setAutoTerminatedRelationships(std::vector<Relationship> relationships) {
    setAutoTerminatedRelationships(std::span(relationships));
  }

  bool isAutoTerminated(const Relationship& relationship) override {
    return processor_->isAutoTerminated(relationship);
  }

  bool isSupportedRelationship(const Relationship& relationship) override {
    return processor_->isSupportedRelationship(relationship);
  }

  /**
   * Set name.
   * @param name
   */
  void setName(std::string name) override {
    ConnectableImpl::setName(name);
    processor_->setName(std::move(name));
  }

  /**
   * Set UUID in this instance
   * @param uuid uuid to apply to the internal representation.
   */
  void setUUID(const utils::Identifier& uuid) override {
    ConnectableImpl::setUUID(uuid);
    processor_->setUUID(uuid);
  }

  std::chrono::milliseconds getPenalizationPeriod() const override {
    return processor_->getPenalizationPeriod();
  }

  /**
   * Get outgoing connection based on relationship
   * @return set of outgoing connections.
   */
  std::set<Connectable*> getOutGoingConnections(const std::string& relationship) override {
    return processor_->getOutGoingConnections(relationship);
  }

  /**
   * Get next incoming connection
   * @return next incoming connection
   */
  Connectable* getNextIncomingConnection() override {
    return processor_->getNextIncomingConnection();
  }

  Connectable* pickIncomingConnection() override {
    return processor_->pickIncomingConnection();
  }

  /**
   * @return true if incoming connections > 0
   */
  bool hasIncomingConnections() const override {
    return processor_->hasIncomingConnections();
  }

  /**
   * Returns the UUID through the provided object.
   * @param uuid uuid struct to which we will copy the memory
   * @return success of request
   */
  utils::Identifier getUUID() const override {
    return processor_->getUUID();
  }

  /**
   * Return the UUID string
   * @return the UUID str
   */
  utils::SmallString<36> getUUIDStr() const override {
    return processor_->getUUIDStr();
  }

// Get Process Name
  std::string getName() const override {
    return processor_->getName();
  }

  uint8_t getMaxConcurrentTasks() const override {
    return processor_->getMaxConcurrentTasks();
  }

  void setMaxConcurrentTasks(uint8_t tasks) override {
    processor_->setMaxConcurrentTasks(tasks);
  }

  bool supportsDynamicProperties() const override {
    return false;
  }

  bool supportsDynamicRelationships() const override {
    return false;
  }

  bool isRunning() const override;

  bool isWorkAvailable() override;

 protected:
  bool canEdit() override {
    return !processor_->isRunning();
  }

  /**
   * internal connectable.
   */
  Connectable* processor_;
};

}  // namespace org::apache::nifi::minifi::core
