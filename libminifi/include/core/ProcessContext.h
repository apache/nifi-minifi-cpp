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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ProcessContext Class
class ProcessContext : public controller::ControllerServiceLookup {
 public:
  // Constructor
  /*!
   * Create a new process context associated with the processor/controller service/state manager
   */
  ProcessContext(const std::shared_ptr<ProcessorNode> &processor, std::shared_ptr<controller::ControllerServiceProvider> &controller_service_provider, const std::shared_ptr<core::Repository> &repo,
                 const std::shared_ptr<core::Repository> &flow_repo, const std::shared_ptr<core::ContentRepository> &content_repo = std::make_shared<core::repository::FileSystemRepository>())
      : processor_node_(processor),
        controller_service_provider_(controller_service_provider),
        logger_(logging::LoggerFactory<ProcessContext>::getLogger()),
        content_repo_(content_repo),
        flow_repo_(flow_repo) {
    repo_ = repo;
  }
  // Destructor
  virtual ~ProcessContext() {
  }
  // Get Processor associated with the Process Context
  std::shared_ptr<ProcessorNode> getProcessorNode() {
    return processor_node_;
  }
  bool getProperty(const std::string &name, std::string &value) {
    return processor_node_->getProperty(name, value);
  }
  bool getProperty(const std::string &name, std::string &value, const std::shared_ptr<FlowFile> &flow_file);
  // Sets the property value using the property's string name
  bool setProperty(const std::string &name, std::string value) {
    return processor_node_->setProperty(name, value);
  }
  // Sets the property value using the Property object
  bool setProperty(Property prop, std::string value) {
    return processor_node_->setProperty(prop, value);
  }
  // Whether the relationship is supported
  bool isSupportedRelationship(Relationship relationship) {
    return processor_node_->isSupportedRelationship(relationship);
  }

  // Check whether the relationship is auto terminated
  bool isAutoTerminated(Relationship relationship) {
    return processor_node_->isAutoTerminated(relationship);
  }
  // Get ProcessContext Maximum Concurrent Tasks
  uint8_t getMaxConcurrentTasks(void) {
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
  std::shared_ptr<core::ContentRepository> getContentRepository() {
    return content_repo_;
  }

  std::shared_ptr<core::Repository> getFlowFileRepository() {
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
    return controller_service_provider_->getControllerServiceForComponent(identifier, processor_node_->getUUIDStr());
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

  // Logger
  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
