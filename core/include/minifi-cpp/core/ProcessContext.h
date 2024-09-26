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

#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/ContentRepository.h"
#include "minifi-cpp/core/controller/ControllerServiceLookup.h"
#include "minifi-cpp/core/ProcessorNode.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/core/Repository.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/StateStorage.h"
#include "minifi-cpp/core/VariableRegistry.h"

namespace org::apache::nifi::minifi::core {

namespace detail {
template<typename T>
concept NotAFlowFile = !std::convertible_to<T &, const FlowFile &> && !std::convertible_to<T &, const std::shared_ptr<FlowFile> &>;
}  // namespace detail

class ProcessContext : public virtual controller::ControllerServiceLookup, public virtual core::VariableRegistry, public virtual utils::EnableSharedFromThis {
 public:
  virtual std::shared_ptr<ProcessorNode> getProcessorNode() const = 0;
  virtual std::optional<std::string> getProperty(const Property&, const FlowFile* const) = 0;
  virtual std::optional<std::string> getProperty(const PropertyReference&, const FlowFile* const) = 0;
  virtual bool getProperty(const Property &property, std::string &value, const FlowFile* const) = 0;
  virtual bool getProperty(const PropertyReference& property, std::string &value, const FlowFile* const) = 0;
  virtual bool getDynamicProperty(const std::string &name, std::string &value) const = 0;
  virtual bool getDynamicProperty(const Property &property, std::string &value, const FlowFile* const) = 0;
  virtual std::vector<std::string> getDynamicPropertyKeys() const = 0;
  virtual bool setProperty(const std::string &name, std::string value) = 0;
  virtual bool setDynamicProperty(const std::string &name, std::string value) = 0;
  virtual bool setProperty(const Property& property, std::string value) = 0;
  virtual bool setProperty(const PropertyReference& property, std::string_view value) = 0;
  virtual bool isAutoTerminated(Relationship relationship) const = 0;
  virtual uint8_t getMaxConcurrentTasks() const = 0;
  virtual void yield() = 0;
  virtual std::shared_ptr<core::Repository> getProvenanceRepository() = 0;
  virtual std::shared_ptr<core::ContentRepository> getContentRepository() const = 0;
  virtual std::shared_ptr<core::Repository> getFlowFileRepository() const = 0;
  virtual void initializeContentRepository(const std::string& home) = 0;
  virtual bool isInitialized() const = 0;

  static constexpr char const* DefaultStateStorageName = "defaultstatestorage";

  virtual StateManager* getStateManager() = 0;
  virtual bool hasStateManager() const = 0;
  virtual std::shared_ptr<Configure> getConfiguration() const = 0;



  template<std::default_initializable T = std::string>
  std::optional<T> getProperty(const Property& property) const;

  template<std::default_initializable T = std::string>
  std::optional<T> getProperty(const PropertyReference& property) const;

  bool getProperty(std::string_view name, detail::NotAFlowFile auto& value) const;

  bool getProperty(const PropertyReference& property, detail::NotAFlowFile auto& value) const;
};

}  // namespace org::apache::nifi::minifi::core
