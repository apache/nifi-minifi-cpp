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
#include <concepts>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "controllers/keyvalue/KeyValueStateStorage.h"
#include "core/Core.h"
#include "core/ContentRepository.h"
#include "core/repository/FileSystemRepository.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ControllerServiceLookup.h"
#include "core/logging/LoggerFactory.h"
#include "core/ProcessorNode.h"
#include "core/Property.h"
#include "core/PropertyDefinition.h"
#include "core/Repository.h"
#include "core/FlowFile.h"
#include "core/StateStorage.h"
#include "core/VariableRegistry.h"
#include "utils/file/FileUtils.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::core {

namespace detail {
template<typename T>
concept NotAFlowFile = !std::convertible_to<T &, const FlowFile &> && !std::convertible_to<T &, const std::shared_ptr<FlowFile> &>;
}  // namespace detail

class ProcessContext : public controller::ControllerServiceLookup, public core::VariableRegistry, public std::enable_shared_from_this<VariableRegistry> {
 public:
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
    state_storage_ = getStateStorage(logger_, controller_service_provider_, nullptr);
  }

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
    state_storage_ = getStateStorage(logger_, controller_service_provider_, configuration);
    if (!configure_) {
      configure_ = std::make_shared<minifi::Configure>();
    }
  }

  // Get Processor associated with the Process Context
  std::shared_ptr<ProcessorNode> getProcessorNode() const {
    return processor_node_;
  }

  std::optional<std::string> getProperty(const Property&, const FlowFile* const);

  std::optional<std::string> getProperty(const PropertyReference&, const FlowFile* const);

  virtual bool getProperty(const Property &property, std::string &value, const FlowFile* const) {
    return getProperty(property.getName(), value);
  }

  virtual bool getProperty(const PropertyReference& property, std::string &value, const FlowFile* const) {
    return getProperty(property.name, value);
  }

  bool getDynamicProperty(const std::string &name, std::string &value) const {
    return processor_node_->getDynamicProperty(name, value);
  }
  virtual bool getDynamicProperty(const Property &property, std::string &value, const FlowFile* const) {
    return getDynamicProperty(property.getName(), value);
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
  bool setProperty(const PropertyReference& property, std::string_view value) {
    return setProperty(std::string{property.name}, std::string{value});
  }
  // Check whether the relationship is auto terminated
  bool isAutoTerminated(Relationship relationship) const {
    return processor_node_->isAutoTerminated(relationship);
  }
  // Get ProcessContext Maximum Concurrent Tasks
  uint8_t getMaxConcurrentTasks() const {
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
    return controller_service_provider_ == nullptr ? nullptr : controller_service_provider_->getControllerService(identifier);
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

  static constexpr char const* DefaultStateStorageName = "defaultstatestorage";

  StateManager* getStateManager() {
    if (state_storage_ == nullptr) {
      return nullptr;
    }
    if (!state_manager_) {
      state_manager_ = state_storage_->getStateManager(*processor_node_);
    }
    return state_manager_.get();
  }

  bool hasStateManager() const {
    return state_manager_ != nullptr;
  }

  static std::shared_ptr<core::StateStorage> getOrCreateDefaultStateStorage(
      controller::ControllerServiceProvider* controller_service_provider,
      const std::shared_ptr<minifi::Configure>& configuration) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    /* See if we have already created a default provider */
    core::controller::ControllerServiceNode* node = controller_service_provider->getControllerServiceNode(DefaultStateStorageName);
    if (node != nullptr) {
      return std::dynamic_pointer_cast<core::StateStorage>(node->getControllerServiceImplementation());
    }

    /* Try to get configuration options for default provider */
    std::string always_persist;
    configuration->get(Configure::nifi_state_storage_local_always_persist, Configure::nifi_state_storage_local_always_persist_old, always_persist);
    std::string auto_persistence_interval;
    configuration->get(Configure::nifi_state_storage_local_auto_persistence_interval, Configure::nifi_state_storage_local_auto_persistence_interval_old, auto_persistence_interval);

    const auto path = configuration->getWithFallback(Configure::nifi_state_storage_local_path, Configure::nifi_state_storage_local_path_old);

    /* Function to help creating a state storage */
    auto create_provider = [&](
        const std::string& type,
        const std::string& longType,
        const std::unordered_map<std::string, std::string>& extraProperties) -> std::shared_ptr<core::StateStorage> {
      auto new_node = controller_service_provider->createControllerService(type, longType, DefaultStateStorageName, true /*firstTimeAdded*/);
      if (new_node == nullptr) {
        return nullptr;
      }
      new_node->initialize();
      auto storage = new_node->getControllerServiceImplementation();
      if (storage == nullptr) {
        return nullptr;
      }
      if (!always_persist.empty() && !storage->setProperty(controllers::ALWAYS_PERSIST_PROPERTY_NAME, always_persist)) {
        return nullptr;
      }
      if (!auto_persistence_interval.empty() && !storage->setProperty(controllers::AUTO_PERSISTENCE_INTERVAL_PROPERTY_NAME, auto_persistence_interval)) {
        return nullptr;
      }
      for (const auto& extraProperty : extraProperties) {
        if (!storage->setProperty(extraProperty.first, extraProperty.second)) {
          return nullptr;
        }
      }
      if (!new_node->enable()) {
        return nullptr;
      }
      return std::dynamic_pointer_cast<core::StateStorage>(storage);
    };

    std::string preferredType;
    configuration->get(minifi::Configure::nifi_state_storage_local_class_name, minifi::Configure::nifi_state_storage_local_class_name_old, preferredType);

    /* Try to create a RocksDB-backed provider */
    if (preferredType.empty() || preferredType == "RocksDbPersistableKeyValueStoreService" || preferredType == "RocksDbStateStorage") {
      auto provider = create_provider("RocksDbStateStorage",
                                      "org.apache.nifi.minifi.controllers.RocksDbStateStorage",
                                      {{"Directory", path.value_or("corecomponentstate")}});
      if (provider != nullptr) {
        return provider;
      }
    }

    /* Fall back to a locked unordered map-backed provider */
    if (preferredType.empty() || preferredType == "UnorderedMapPersistableKeyValueStoreService" || preferredType == "PersistentMapStateStorage") {
      auto provider = create_provider("PersistentMapStateStorage",
                                 "org.apache.nifi.minifi.controllers.PersistentMapStateStorage",
                                 {{"File", path.value_or("corecomponentstate.txt")}});
      if (provider != nullptr) {
        return provider;
      }
    }

    /* Fall back to volatile memory-backed provider */
    if (preferredType.empty() || preferredType == "UnorderedMapKeyValueStoreService" || preferredType == "VolatileMapStateStorage") {
      auto provider = create_provider("VolatileMapStateStorage",
                                      "org.apache.nifi.minifi.controllers.VolatileMapStateStorage",
                                      {});
      if (provider != nullptr) {
        return provider;
      }
    }

    /* Give up */
    return nullptr;
  }

  static std::shared_ptr<core::StateStorage> getStateStorage(
      const std::shared_ptr<logging::Logger>& logger,
      controller::ControllerServiceProvider* const controller_service_provider,
      const std::shared_ptr<minifi::Configure>& configuration) {
    if (controller_service_provider == nullptr) {
      return nullptr;
    }
    std::string requestedStateStorageName;
    if (configuration != nullptr && configuration->get(minifi::Configure::nifi_state_storage_local, minifi::Configure::nifi_state_storage_local_old, requestedStateStorageName)) {
      auto node = controller_service_provider->getControllerServiceNode(requestedStateStorageName);
      if (node == nullptr) {
        logger->log_error("Failed to find the StateStorage {} defined by {}", requestedStateStorageName, minifi::Configure::nifi_state_storage_local);
        return nullptr;
      }
      return std::dynamic_pointer_cast<core::StateStorage>(node->getControllerServiceImplementation());
    } else {
      auto state_storage = getOrCreateDefaultStateStorage(controller_service_provider, configuration);
      if (state_storage == nullptr) {
        logger->log_error("Failed to create default StateStorage");
      }
      return state_storage;
    }
  }

  std::shared_ptr<Configure> getConfiguration() const {
    return configure_;
  }

 private:
  template<typename T>
  bool getPropertyImp(const std::string &name, T &value) const {
    return processor_node_->getProperty<typename std::common_type<T>::type>(name, value);
  }

  controller::ControllerServiceProvider* controller_service_provider_;
  std::shared_ptr<core::StateStorage> state_storage_;
  std::unique_ptr<StateManager> state_manager_;
  std::shared_ptr<core::Repository> repo_;
  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<ProcessorNode> processor_node_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<Configure> configure_;
  bool initialized_;
};

inline std::optional<std::string> ProcessContext::getProperty(const Property& property, const FlowFile* const flow_file) {
  std::string value;
  if (!getProperty(property, value, flow_file)) return std::nullopt;
  return value;
}

inline std::optional<std::string> ProcessContext::getProperty(const PropertyReference& property, const FlowFile* const flow_file) {
  std::string value;
  if (!getProperty(property, value, flow_file)) return std::nullopt;
  return value;
}

}  // namespace org::apache::nifi::minifi::core
