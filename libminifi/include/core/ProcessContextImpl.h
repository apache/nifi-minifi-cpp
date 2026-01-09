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

#include "core/ConfigurableComponentImpl.h"
#include "minifi-cpp/core/ContentRepository.h"
#include "core/Core.h"
#include "core/VariableRegistry.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/controllers/keyvalue/KeyValueStateStorage.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/Repository.h"
#include "minifi-cpp/core/StateStorage.h"
#include "minifi-cpp/core/controller/ControllerServiceLookup.h"
#include "minifi-cpp/core/controller/ControllerServiceProvider.h"
#include "minifi-cpp/core/repository/FileSystemRepository.h"
#include "expression-language/Expression.h"

namespace org::apache::nifi::minifi::core {

class Processor;

class ProcessContextImpl : public core::VariableRegistryImpl, public virtual ProcessContext {
 public:
  ProcessContextImpl(Processor& processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository>& repo,
      const std::shared_ptr<core::Repository>& flow_repo, const std::shared_ptr<core::ContentRepository>& content_repo = repository::createFileSystemRepository());

  ProcessContextImpl(Processor& processor, controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<core::Repository>& repo,
      const std::shared_ptr<core::Repository>& flow_repo, const std::shared_ptr<minifi::Configure>& configuration,
      const std::shared_ptr<core::ContentRepository>& content_repo = repository::createFileSystemRepository());

  // Get Processor associated with the Process Context
  Processor& getProcessor() const override { return processor_; }

  const ProcessorInfo& getProcessorInfo() const override { return *info_; }

  bool hasNonEmptyProperty(std::string_view name) const override;

  bool isRunning() const override;
  nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile*) const override;
  nonstd::expected<void, std::error_code> setProperty(std::string_view name, std::string value) override;
  nonstd::expected<void, std::error_code> clearProperty(std::string_view name) override;
  nonstd::expected<std::string, std::error_code> getDynamicProperty(std::string_view name, const FlowFile*) const override;
  nonstd::expected<void, std::error_code> setDynamicProperty(std::string name, std::string value) override;
  nonstd::expected<std::string, std::error_code> getRawProperty(std::string_view name) const override;
  nonstd::expected<std::string, std::error_code> getRawDynamicProperty(std::string_view name) const override;
  [[nodiscard]] nonstd::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const override;
  bool hasIncomingConnections() const override;

  void addAutoTerminatedRelationship(const core::Relationship& relationship) override;

  std::vector<std::string> getDynamicPropertyKeys() const override;
  std::map<std::string, std::string> getDynamicProperties(const FlowFile*) const override;

  bool isAutoTerminated(Relationship relationship) const override;
  uint8_t getMaxConcurrentTasks() const override;

  void yield() override;

  std::shared_ptr<core::Repository> getProvenanceRepository() override { return repo_; }

  /**
   * Returns a reference to the content repository for the running instance.
   * @return content repository shared pointer.
   */
  std::shared_ptr<core::ContentRepository> getContentRepository() const override { return content_repo_; }

  std::shared_ptr<core::Repository> getFlowFileRepository() const override { return flow_repo_; }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProcessContextImpl(const ProcessContextImpl& parent) = delete;
  ProcessContextImpl& operator=(const ProcessContextImpl& parent) = delete;

  // controller services

  std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const override {
    auto controller_service = controller_service_provider_ == nullptr ? nullptr : controller_service_provider_->getControllerService(identifier, processor_uuid);
    if (!controller_service || controller_service->getState() != core::controller::ControllerServiceState::ENABLED) {
      return nullptr;
    }
    return controller_service;
  }

  static constexpr char const* DefaultStateStorageName = "defaultstatestorage";

  StateManager* getStateManager() override;

  bool hasStateManager() const override { return state_manager_ != nullptr; }

  static std::shared_ptr<core::StateStorage> getOrCreateDefaultStateStorage(
      controller::ControllerServiceProvider* controller_service_provider, const std::shared_ptr<minifi::Configure>& configuration) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    /* See if we have already created a default provider */
    core::controller::ControllerServiceNode* node = controller_service_provider->getControllerServiceNode(DefaultStateStorageName);
    if (node != nullptr) { return std::dynamic_pointer_cast<core::StateStorage>(node->getControllerServiceImplementation()); }

    /* Try to get configuration options for default provider */
    std::string always_persist;
    configuration->get(Configure::nifi_state_storage_local_always_persist, Configure::nifi_state_storage_local_always_persist_old, always_persist);
    std::string auto_persistence_interval;
    configuration->get(Configure::nifi_state_storage_local_auto_persistence_interval, Configure::nifi_state_storage_local_auto_persistence_interval_old, auto_persistence_interval);

    const auto path = configuration->getWithFallback(Configure::nifi_state_storage_local_path, Configure::nifi_state_storage_local_path_old);

    /* Function to help creating a state storage */
    auto create_provider = [&](const std::string& type, const std::unordered_map<std::string, std::string>& extraProperties) -> std::shared_ptr<core::StateStorage> {
      auto new_node = controller_service_provider->createControllerService(type, DefaultStateStorageName);
      if (new_node == nullptr) { return nullptr; }
      new_node->initialize();
      auto storage = new_node->getControllerServiceImplementation();
      if (storage == nullptr) { return nullptr; }
      if (!always_persist.empty() && !storage->setProperty(controllers::ALWAYS_PERSIST_PROPERTY_NAME, always_persist)) { return nullptr; }
      if (!auto_persistence_interval.empty() && !storage->setProperty(controllers::AUTO_PERSISTENCE_INTERVAL_PROPERTY_NAME, auto_persistence_interval)) { return nullptr; }
      for (const auto& extraProperty: extraProperties) {
        if (!storage->setProperty(extraProperty.first, extraProperty.second)) { return nullptr; }
      }
      if (!new_node->enable()) { return nullptr; }
      return std::dynamic_pointer_cast<core::StateStorage>(storage);
    };

    std::string preferredType;
    configuration->get(minifi::Configure::nifi_state_storage_local_class_name, minifi::Configure::nifi_state_storage_local_class_name_old, preferredType);

    /* Try to create a RocksDB-backed provider */
    if (preferredType.empty() || preferredType == "RocksDbPersistableKeyValueStoreService" || preferredType == "RocksDbStateStorage") {
      auto provider = create_provider("RocksDbStateStorage", {{"Directory", path.value_or("corecomponentstate")}});
      if (provider != nullptr) { return provider; }
    }

    /* Fall back to a locked unordered map-backed provider */
    if (preferredType.empty() || preferredType == "UnorderedMapPersistableKeyValueStoreService" || preferredType == "PersistentMapStateStorage") {
      auto provider = create_provider("PersistentMapStateStorage", {{"File", path.value_or("corecomponentstate.txt")}});
      if (provider != nullptr) { return provider; }
    }

    /* Fall back to volatile memory-backed provider */
    if (preferredType.empty() || preferredType == "UnorderedMapKeyValueStoreService" || preferredType == "VolatileMapStateStorage") {
      auto provider = create_provider("VolatileMapStateStorage", {});
      if (provider != nullptr) { return provider; }
    }

    /* Give up */
    return nullptr;
  }

  static std::shared_ptr<core::StateStorage> getStateStorage(
      const std::shared_ptr<logging::Logger>& logger, controller::ControllerServiceProvider* const controller_service_provider, const std::shared_ptr<minifi::Configure>& configuration) {
    if (controller_service_provider == nullptr) { return nullptr; }
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
      if (state_storage == nullptr) { logger->log_error("Failed to create default StateStorage"); }
      return state_storage;
    }
  }

  gsl::not_null<Configure*> getConfiguration() const override { return gsl::make_not_null(configure_.get()); }

 private:
  std::shared_ptr<logging::Logger> logger_;
  controller::ControllerServiceProvider* controller_service_provider_;
  std::shared_ptr<core::StateStorage> state_storage_;
  std::unique_ptr<StateManager> state_manager_;
  std::shared_ptr<core::Repository> repo_;
  std::shared_ptr<core::Repository> flow_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  Processor& processor_;
  gsl::not_null<std::shared_ptr<Configure>> configure_;
  std::unique_ptr<ProcessorInfo> info_;

  // each ProcessContextImpl instance is only accessed from one thread at a time, so no synchronization is needed on these caches
  mutable std::unordered_map<std::string, expression::Expression, utils::string::transparent_string_hash, std::equal_to<>> cached_expressions_;
  mutable std::unordered_map<std::string, expression::Expression, utils::string::transparent_string_hash, std::equal_to<>> cached_dynamic_expressions_;
};

}  // namespace org::apache::nifi::minifi::core
