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

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include <unordered_map>

#include "Property.h"
#include "core/Core.h"
#include "core/ContentRepository.h"
#include "core/repository/FileSystemRepository.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ControllerServiceLookup.h"
#include "core/logging/LoggerFactory.h"
#include "controllers/keyvalue/AbstractAutoPersistingKeyValueStoreService.h"
#include "ProcessorNode.h"
#include "core/Repository.h"
#include "core/FlowFile.h"
#include "core/CoreComponentState.h"
#include "utils/file/FileUtils.h"
#include "utils/PropertyErrors.h"
#include "VariableRegistry.h"

namespace org::apache::nifi::minifi::core {

// ProcessContext Class
class ProcessContext : public controller::ControllerServiceLookup, public core::VariableRegistry, public std::enable_shared_from_this<VariableRegistry> {
 public:
  // Constructor
  /*!
   * Create a new process context associated with the processor/controller service/state manager
   */
  ProcessContext(const std::shared_ptr<ProcessorNode> &processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository> &repo,
                 const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<core::ContentRepository> &content_repo = std::make_shared<core::repository::FileSystemRepository>())
      : VariableRegistry(std::make_shared<minifi::Configure>()),
        controller_service_provider_(controller_service_provider),
        flow_repo_(flow_repo),
        content_repo_(content_repo),
        processor_node_(processor),
        logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
        configure_(std::make_shared<minifi::Configure>()),
        initialized_(false) {
    repo_ = repo;
    state_manager_provider_ = getStateManagerProvider(logger_, controller_service_provider_, nullptr);
  }

  // Constructor
  /*!
   * Create a new process context associated with the processor/controller service/state manager
   */
  ProcessContext(const std::shared_ptr<ProcessorNode> &processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository> &repo,
                 const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<minifi::Configure> &configuration, const std::shared_ptr<core::ContentRepository> &content_repo =
                     std::make_shared<core::repository::FileSystemRepository>())
      : VariableRegistry(configuration),
        controller_service_provider_(controller_service_provider),
        flow_repo_(flow_repo),
        content_repo_(content_repo),
        processor_node_(processor),
        logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
        configure_(configuration),
        initialized_(false) {
    repo_ = repo;
    state_manager_provider_ = getStateManagerProvider(logger_, controller_service_provider_, configuration);
    if (!configure_) {
      configure_ = std::make_shared<minifi::Configure>();
    }
  }
  // Destructor
  virtual ~ProcessContext() = default;
  // Get Processor associated with the Process Context
  std::shared_ptr<ProcessorNode> getProcessorNode() const {
    return processor_node_;
  }

  template<typename T = std::string>
  std::enable_if_t<std::is_default_constructible<T>::value, std::optional<T>>
  getProperty(const Property& property) const {
    T value;
    try {
      if (!getProperty(property.getName(), value)) return std::nullopt;
    } catch (const utils::internal::ValueException&) {
      return std::nullopt;
    }
    return value;
  }

  template<typename T>
  std::enable_if_t<!std::is_convertible_v<T&, const FlowFile&> && !std::is_convertible_v<T&, const std::shared_ptr<FlowFile>&>,
      bool> getProperty(const std::string &name, T &value) const {
    return getPropertyImp<typename std::common_type<T>::type>(name, value);
  }

  template<typename T = std::string>
  std::enable_if_t<std::is_default_constructible_v<T>, std::optional<T>> getProperty(const Property&, const std::shared_ptr<FlowFile>&);

  virtual bool getProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile>& /*flow_file*/) {
    return getProperty(property.getName(), value);
  }
  bool getDynamicProperty(const std::string &name, std::string &value) const {
    return processor_node_->getDynamicProperty(name, value);
  }
  virtual bool getDynamicProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile>& /*flow_file*/) {
    return getDynamicProperty(property.getName(), value);
  }
  bool getDynamicProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile>& flow_file, const std::map<std::string, std::string>& variables) {
    std::map<std::string, std::optional<std::string>> original_attributes;
    for (const auto& [variable, attr_value] : variables) {
      original_attributes[variable] = flow_file->getAttribute(variable);
      flow_file->setAttribute(variable, attr_value);
    }
    auto onExit = gsl::finally([&]{
      for (const auto& attr : original_attributes) {
        if (attr.second) {
          flow_file->setAttribute(attr.first, attr.second.value());
        } else {
          flow_file->removeAttribute(attr.first);
        }
      }
    });
    return getDynamicProperty(property, value, flow_file);
  }
  std::vector<std::string> getDynamicPropertyKeys() const {
    return processor_node_->getDynamicPropertyKeys();
  }
  // Sets the property value using the property's string name
  virtual bool setProperty(const std::string &name, std::string value) {
    return processor_node_->setProperty(name, value);
  }  // Sets the dynamic property value using the property's string name
  virtual bool setDynamicProperty(const std::string &name, std::string value) {
    return processor_node_->setDynamicProperty(name, value);
  }
  // Sets the property value using the Property object
  bool setProperty(const Property& property, std::string value) {
    return setProperty(property.getName(), value);
  }
  // Whether the relationship is supported
  bool isSupportedRelationship(Relationship relationship) const {
    return processor_node_->isSupportedRelationship(relationship);
  }

  // Check whether the relationship is auto terminated
  bool isAutoTerminated(Relationship relationship) const {
    return processor_node_->isAutoTerminated(relationship);
  }
  // Get ProcessContext Maximum Concurrent Tasks
  uint8_t getMaxConcurrentTasks(void) const {
    return processor_node_->getMaxConcurrentTasks();
  }
  // Yield based on the yield period
  void yield() {
    processor_node_->yield();
  }

  std::shared_ptr<core::Repository> getProvenanceRepository() {
    return repo_;
  }

  /**
   * Returns a reference to the content repository for the running instance.
   * @return content repository shared pointer.
   */
  std::shared_ptr<core::ContentRepository> getContentRepository() const {
    return content_repo_;
  }

  std::shared_ptr<core::Repository> getFlowFileRepository() const {
    return flow_repo_;
  }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProcessContext(const ProcessContext &parent) = delete;
  ProcessContext &operator=(const ProcessContext &parent) = delete;

  // controller services

  /**
   * @param identifier of controller service
   * @return the ControllerService that is registered with the given
   * identifier
   */
  std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string &identifier) const override {
    return controller_service_provider_ == nullptr ? nullptr : controller_service_provider_->getControllerServiceForComponent(identifier, processor_node_->getUUID());
  }

  /**
   * @param identifier identifier of service to check
   * @return <code>true</code> if the Controller Service with the given
   * identifier is enabled, <code>false</code> otherwise. If the given
   * identifier is not known by this ControllerServiceLookup, returns
   * <code>false</code>
   */
  bool isControllerServiceEnabled(const std::string &identifier) override {
    return controller_service_provider_->isControllerServiceEnabled(identifier);
  }

  /**
   * @param identifier identifier of service to check
   * @return <code>true</code> if the Controller Service with the given
   * identifier has been enabled but is still in the transitioning state,
   * otherwise returns <code>false</code>. If the given identifier is not
   * known by this ControllerServiceLookup, returns <code>false</code>
   */
  bool isControllerServiceEnabling(const std::string &identifier) override {
    return controller_service_provider_->isControllerServiceEnabling(identifier);
  }

  /**
   * @param identifier identifier to look up
   * @return the name of the Controller service with the given identifier. If
   * no service can be found with this identifier, returns {@code null}
   */
  const std::string getControllerServiceName(const std::string &identifier) const override {
    return controller_service_provider_->getControllerServiceName(identifier);
  }

  void initializeContentRepository(const std::string& home) {
      configure_->setHome(home);
      content_repo_->initialize(configure_);
      initialized_ = true;
  }

  bool isInitialized() const {
      return initialized_;
  }

  static constexpr char const* DefaultStateManagerProviderName = "defaultstatemanagerprovider";

  CoreComponentStateManager* getStateManager() {
    if (state_manager_provider_ == nullptr) {
      return nullptr;
    }
    if (!state_manager_) {
      state_manager_ = state_manager_provider_->getCoreComponentStateManager(*processor_node_);
    }
    return state_manager_.get();
  }

  bool hasStateManager() const {
    return state_manager_ != nullptr;
  }

  static std::shared_ptr<core::CoreComponentStateManagerProvider> getOrCreateDefaultStateManagerProvider(
      controller::ControllerServiceProvider* controller_service_provider,
      const std::shared_ptr<minifi::Configure>& configuration) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    /* See if we have already created a default provider */
    std::shared_ptr<core::controller::ControllerServiceNode> node = controller_service_provider->getControllerServiceNode(DefaultStateManagerProviderName);
    if (node != nullptr) {
      return std::dynamic_pointer_cast<core::CoreComponentStateManagerProvider>(node->getControllerServiceImplementation());
    }

    /* Try to get configuration options for default provider */
    std::string always_persist, auto_persistence_interval;
    configuration->get(Configure::nifi_state_management_provider_local_always_persist, always_persist);
    configuration->get(Configure::nifi_state_management_provider_local_auto_persistence_interval, auto_persistence_interval);
    const auto path = configuration->get(Configure::nifi_state_management_provider_local_path);

    /* Function to help creating a provider */
    auto create_provider = [&](
        const std::string& type,
        const std::string& longType,
        const std::unordered_map<std::string, std::string>& extraProperties) -> std::shared_ptr<core::CoreComponentStateManagerProvider> {
      node = controller_service_provider->createControllerService(type, longType, DefaultStateManagerProviderName, true /*firstTimeAdded*/);
      if (node == nullptr) {
        return nullptr;
      }
      node->initialize();
      auto provider = node->getControllerServiceImplementation();
      if (provider == nullptr) {
        return nullptr;
      }
      if (!always_persist.empty() && !provider->setProperty(
          controllers::AbstractAutoPersistingKeyValueStoreService::AlwaysPersistPropertyName, always_persist)) {
        return nullptr;
      }
      if (!auto_persistence_interval.empty() && !provider->setProperty(
          controllers::AbstractAutoPersistingKeyValueStoreService::AutoPersistenceIntervalPropertyName, auto_persistence_interval)) {
        return nullptr;
      }
      for (const auto& extraProperty : extraProperties) {
        if (!provider->setProperty(extraProperty.first, extraProperty.second)) {
          return nullptr;
        }
      }
      if (!node->enable()) {
        return nullptr;
      }
      return std::dynamic_pointer_cast<core::CoreComponentStateManagerProvider>(provider);
    };

    std::string preferredType;
    configuration->get(minifi::Configure::nifi_state_management_provider_local_class_name, preferredType);

    /* Try to create a RocksDB-backed provider */
    if (preferredType.empty() || preferredType == "RocksDbPersistableKeyValueStoreService") {
      auto provider = create_provider("RocksDbPersistableKeyValueStoreService",
                                      "org.apache.nifi.minifi.controllers.RocksDbPersistableKeyValueStoreService",
                                      {{"Directory", path.value_or("corecomponentstate")}});
      if (provider != nullptr) {
        return provider;
      }
    }

    /* Fall back to a locked unordered map-backed provider */
    if (preferredType.empty() || preferredType == "UnorderedMapPersistableKeyValueStoreService") {
      auto provider = create_provider("UnorderedMapPersistableKeyValueStoreService",
                                 "org.apache.nifi.minifi.controllers.UnorderedMapPersistableKeyValueStoreService",
                                 {{"File", path.value_or("corecomponentstate.txt")}});
      if (provider != nullptr) {
        return provider;
      }
    }

    /* Fall back to volatile memory-backed provider */
    if (preferredType.empty() || preferredType == "UnorderedMapKeyValueStoreService") {
      auto provider = create_provider("UnorderedMapKeyValueStoreService",
                                      "org.apache.nifi.minifi.controllers.UnorderedMapKeyValueStoreService",
                                      {});
      if (provider != nullptr) {
        return provider;
      }
    }

    /* Give up */
    return nullptr;
  }

  static std::shared_ptr<core::CoreComponentStateManagerProvider> getStateManagerProvider(
      const std::shared_ptr<logging::Logger>& logger,
      controller::ControllerServiceProvider* const controller_service_provider,
      const std::shared_ptr<minifi::Configure>& configuration) {
    if (controller_service_provider == nullptr) {
      return nullptr;
    }
    std::string requestedStateManagerName;
    if (configuration != nullptr && configuration->get(minifi::Configure::nifi_state_management_provider_local, requestedStateManagerName)) {
      auto node = controller_service_provider->getControllerServiceNode(requestedStateManagerName);
      if (node == nullptr) {
        logger->log_error("Failed to find the CoreComponentStateManagerProvider %s defined by %s", requestedStateManagerName, minifi::Configure::nifi_state_management_provider_local);
        return nullptr;
      }
      return std::dynamic_pointer_cast<core::CoreComponentStateManagerProvider>(node->getControllerServiceImplementation());
    } else {
      auto state_manager_provider = getOrCreateDefaultStateManagerProvider(controller_service_provider, configuration);
      if (state_manager_provider == nullptr) {
        logger->log_error("Failed to create default CoreComponentStateManagerProvider");
      }
      return state_manager_provider;
    }
  }

 private:
  template<typename T>
  bool getPropertyImp(const std::string &name, T &value) const {
    return processor_node_->getProperty<typename std::common_type<T>::type>(name, value);
  }

  controller::ControllerServiceProvider* controller_service_provider_;
  std::shared_ptr<core::CoreComponentStateManagerProvider> state_manager_provider_;
  std::unique_ptr<CoreComponentStateManager> state_manager_;
  std::shared_ptr<core::Repository> repo_;
  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<ProcessorNode> processor_node_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<Configure> configure_;
  bool initialized_;
};

template<typename T>
inline std::enable_if_t<std::is_default_constructible_v<T>, std::optional<T>> ProcessContext::getProperty(const Property& property, const std::shared_ptr<FlowFile>& flow_file) {
  T value;
  std::string string_value;
  if (!getProperty(property, string_value, flow_file)) return std::nullopt;
  try {
    if (!state::response::Value{string_value}.template convertValue<>(value)) return std::nullopt;
  } catch (const utils::internal::ValueException&) {
    return std::nullopt;
  }
  return value;
}

template<>
inline std::optional<std::string> ProcessContext::getProperty<std::string>(const Property& property, const std::shared_ptr<FlowFile>& flow_file) {
  std::string value;
  if (!getProperty(property, value, flow_file)) return std::nullopt;
  return value;
}

}  // namespace org::apache::nifi::minifi::core
