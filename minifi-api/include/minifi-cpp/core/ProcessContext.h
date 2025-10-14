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
#include <optional>
#include <string>
#include <vector>

#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/ContentRepository.h"
#include "minifi-cpp/core/controller/ControllerServiceLookup.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/Repository.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/StateStorage.h"
#include "minifi-cpp/core/VariableRegistry.h"

namespace org::apache::nifi::minifi::core {

class ProcessorInfo {
 public:
  [[nodiscard]] virtual std::string getName() const = 0;
  [[nodiscard]] virtual utils::Identifier getUUID() const = 0;
  [[nodiscard]] virtual std::shared_ptr<state::FlowIdentifier> getFlowIdentifier() const = 0;
  [[nodiscard]] virtual std::map<std::string, core::Property, std::less<>> getSupportedProperties() const = 0;
  [[nodiscard]] virtual nonstd::expected<Property, std::error_code> getSupportedProperty(std::string_view name) const = 0;

  virtual ~ProcessorInfo() = default;
};

class Processor;

class ProcessContext : public virtual core::VariableRegistry, public virtual utils::EnableSharedFromThis {
 public:
  virtual const ProcessorInfo& getProcessorInfo() const = 0;
  virtual Processor& getProcessor() const = 0;

  virtual nonstd::expected<std::string, std::error_code> getProperty(std::string_view name, const FlowFile* flow_file = nullptr) const = 0;
  nonstd::expected<std::string, std::error_code> getProperty(const Property& property, const FlowFile* flow_file = nullptr) const { return getProperty(property.getName(), flow_file); }
  nonstd::expected<std::string, std::error_code> getProperty(const PropertyReference& property_reference, const FlowFile* flow_file = nullptr) const {
    return getProperty(property_reference.name, flow_file);
  }

  virtual nonstd::expected<std::string, std::error_code> getRawProperty(std::string_view name) const = 0;

  virtual nonstd::expected<void, std::error_code> setProperty(std::string_view name, std::string value) = 0;
  nonstd::expected<void, std::error_code> setProperty(const Property& property, std::string value) { return setProperty(property.getName(), std::move(value)); };
  nonstd::expected<void, std::error_code> setProperty(const PropertyReference& property_reference, std::string value) { return setProperty(property_reference.name, std::move(value)); };

  virtual nonstd::expected<void, std::error_code> clearProperty(std::string_view name) = 0;

  virtual nonstd::expected<std::string, std::error_code> getDynamicProperty(std::string_view name, const FlowFile* flow_file = nullptr) const = 0;
  virtual nonstd::expected<void, std::error_code> setDynamicProperty(std::string name, std::string value) = 0;

  virtual nonstd::expected<std::string, std::error_code> getRawDynamicProperty(std::string_view name) const = 0;

  virtual std::vector<std::string> getDynamicPropertyKeys() const = 0;
  virtual std::map<std::string, std::string> getDynamicProperties(const FlowFile* flow_file = nullptr) const = 0;

  [[nodiscard]] virtual nonstd::expected<std::vector<std::string>, std::error_code> getAllPropertyValues(std::string_view name) const = 0;

  virtual void addAutoTerminatedRelationship(const core::Relationship& relationship) = 0;

  virtual bool isRunning() const = 0;
  virtual bool isAutoTerminated(Relationship relationship) const = 0;
  virtual uint8_t getMaxConcurrentTasks() const = 0;
  virtual std::shared_ptr<core::Repository> getProvenanceRepository() = 0;
  virtual std::shared_ptr<core::ContentRepository> getContentRepository() const = 0;
  virtual std::shared_ptr<core::Repository> getFlowFileRepository() const = 0;

  virtual bool hasIncomingConnections() const = 0;

  virtual bool hasNonEmptyProperty(std::string_view name) const = 0;
  virtual void yield() = 0;

  virtual std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string &identifier, const utils::Identifier &processor_uuid) const = 0;
  static constexpr char const* DefaultStateStorageName = "defaultstatestorage";

  virtual StateManager* getStateManager() = 0;
  virtual bool hasStateManager() const = 0;
  virtual gsl::not_null<Configure*> getConfiguration() const = 0;
};

}  // namespace org::apache::nifi::minifi::core
