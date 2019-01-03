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
#ifndef __PROCESS_CONTEXT_H__
#define __PROCESS_CONTEXT_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <memory>
#include <expression/Expression.h>
#include "Property.h"
#include "core/ContentRepository.h"
#include "core/repository/FileSystemRepository.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ControllerServiceLookup.h"
#include "core/logging/LoggerConfiguration.h"
#include "ProcessorNode.h"
#include "core/Repository.h"
#include "core/FlowFile.h"
#include "VariableRegistry.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ProcessContext Class
class ProcessContext : public controller::ControllerServiceLookup, public core::VariableRegistry, public std::enable_shared_from_this<VariableRegistry> {
 public:

  // Constructor
  /*!
   * Create a new process context associated with the processor/controller service/state manager
   */
  ProcessContext(const std::shared_ptr<ProcessorNode> &processor, std::shared_ptr<controller::ControllerServiceProvider> &controller_service_provider, const std::shared_ptr<core::Repository> &repo,
                 const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<core::ContentRepository> &content_repo = std::make_shared<core::repository::FileSystemRepository>())
      : VariableRegistry(std::make_shared<minifi::Configure>()),
        controller_service_provider_(controller_service_provider),
        flow_repo_(flow_repo),
        content_repo_(content_repo),
        processor_node_(processor),
        logger_(logging::LoggerFactory<ProcessContext>::getLogger()) {
    repo_ = repo;
  }

  // Constructor
  /*!
   * Create a new process context associated with the processor/controller service/state manager
   */
  ProcessContext(const std::shared_ptr<ProcessorNode> &processor, std::shared_ptr<controller::ControllerServiceProvider> &controller_service_provider, const std::shared_ptr<core::Repository> &repo,
                 const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<minifi::Configure> &configuration, const std::shared_ptr<core::ContentRepository> &content_repo =
                     std::make_shared<core::repository::FileSystemRepository>())
      : VariableRegistry(configuration),
        controller_service_provider_(controller_service_provider),
        flow_repo_(flow_repo),
        content_repo_(content_repo),
        processor_node_(processor),
        logger_(logging::LoggerFactory<ProcessContext>::getLogger()) {
    repo_ = repo;
  }
  // Destructor
  virtual ~ProcessContext() {
  }
  // Get Processor associated with the Process Context
  std::shared_ptr<ProcessorNode> getProcessorNode() const {
    return processor_node_;
  }

  template<typename T>
  bool getProperty(const std::string &name, T &value) const {
    return getPropertyImp<typename std::common_type<T>::type>(name, value);
  }

  bool getProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile> &flow_file);
  bool getDynamicProperty(const std::string &name, std::string &value) const {
    return processor_node_->getDynamicProperty(name, value);
  }
  bool getDynamicProperty(const Property &property, std::string &value, const std::shared_ptr<FlowFile> &flow_file);
  std::vector<std::string> getDynamicPropertyKeys() const {
    return processor_node_->getDynamicPropertyKeys();
  }
  // Sets the property value using the property's string name
  bool setProperty(const std::string &name, std::string value) {
    return processor_node_->setProperty(name, value);
  }  // Sets the dynamic property value using the property's string name
  bool setDynamicProperty(const std::string &name, std::string value) {
    return processor_node_->setDynamicProperty(name, value);
  }
  // Sets the property value using the Property object
  bool setProperty(Property prop, std::string value) {
    return processor_node_->setProperty(prop, value);
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
  std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string &identifier) {
    if (controller_service_provider_ != nullptr)
      return controller_service_provider_->getControllerServiceForComponent(identifier, processor_node_->getUUIDStr());
    return nullptr;
  }

  /**
   * @param identifier identifier of service to check
   * @return <code>true</code> if the Controller Service with the given
   * identifier is enabled, <code>false</code> otherwise. If the given
   * identifier is not known by this ControllerServiceLookup, returns
   * <code>false</code>
   */
  bool isControllerServiceEnabled(const std::string &identifier) {
    return controller_service_provider_->isControllerServiceEnabled(identifier);
  }

  /**
   * @param identifier identifier of service to check
   * @return <code>true</code> if the Controller Service with the given
   * identifier has been enabled but is still in the transitioning state,
   * otherwise returns <code>false</code>. If the given identifier is not
   * known by this ControllerServiceLookup, returns <code>false</code>
   */
  bool isControllerServiceEnabling(const std::string &identifier) {
    return controller_service_provider_->isControllerServiceEnabling(identifier);
  }

  /**
   * @param identifier identifier to look up
   * @return the name of the Controller service with the given identifier. If
   * no service can be found with this identifier, returns {@code null}
   */
  const std::string getControllerServiceName(const std::string &identifier) {
    return controller_service_provider_->getControllerServiceName(identifier);
  }

 private:

  template<typename T>
  bool getPropertyImp(const std::string &name, T &value) const {
    return processor_node_->getProperty<typename std::common_type<T>::type>(name, value);
  }

  // controller service provider.
  std::shared_ptr<controller::ControllerServiceProvider> controller_service_provider_;
  // repository shared pointer.
  std::shared_ptr<core::Repository> repo_;
  std::shared_ptr<core::Repository> flow_repo_;

  // repository shared pointer.
  std::shared_ptr<core::ContentRepository> content_repo_;
  // Processor
  std::shared_ptr<ProcessorNode> processor_node_;

  std::map<std::string, org::apache::nifi::minifi::expression::Expression> expressions_;
  std::map<std::string, org::apache::nifi::minifi::expression::Expression> dynamic_property_expressions_;

  // Logger
  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
