/**
 * @file FlowController.h
 * FlowController class declaration
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
#ifndef __FLOW_CONTROLLER_H__
#define __FLOW_CONTROLLER_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>
#include "properties/Configure.h"
#include "core/Relationship.h"
#include "FlowFileRecord.h"
#include "Connection.h"
#include "core/Processor.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessGroup.h"
#include "core/FlowConfiguration.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/controller/ControllerServiceProvider.h"
#include "TimerDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"

#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// Default NiFi Root Group Name
#define DEFAULT_ROOT_GROUP_NAME ""

/**
 * Flow Controller class. Generally used by FlowController factory
 * as a singleton.
 */
class FlowController : public core::controller::ControllerServiceProvider,
    public std::enable_shared_from_this<FlowController> {
 public:
  static const int DEFAULT_MAX_TIMER_DRIVEN_THREAD = 10;
  static const int DEFAULT_MAX_EVENT_DRIVEN_THREAD = 5;

  /**
   * Flow controller constructor
   */
  FlowController(std::shared_ptr<core::Repository> provenance_repo,
                 std::shared_ptr<core::Repository> flow_file_repo,
                 std::shared_ptr<Configure> configure,
                 std::unique_ptr<core::FlowConfiguration> flow_configuration,
                 const std::string name = DEFAULT_ROOT_GROUP_NAME,
                 bool headless_mode = false);

  // Destructor
  virtual ~FlowController();

  // Set MAX TimerDrivenThreads
  virtual void setMaxTimerDrivenThreads(int number) {
    max_timer_driven_threads_ = number;
  }
  // Get MAX TimerDrivenThreads
  virtual int getMaxTimerDrivenThreads() {
    return max_timer_driven_threads_;
  }
  // Set MAX EventDrivenThreads
  virtual void setMaxEventDrivenThreads(int number) {
    max_event_driven_threads_ = number;
  }
  // Get MAX EventDrivenThreads
  virtual int getMaxEventDrivenThreads() {
    return max_event_driven_threads_;
  }
  // Get the provenance repository
  virtual std::shared_ptr<core::Repository> getProvenanceRepository() {
    return this->provenance_repo_;
  }

  // Get the flowfile repository
  virtual std::shared_ptr<core::Repository> getFlowFileRepository() {
    return this->flow_file_repo_;
  }

  // Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  virtual void load();

  // Whether the Flow Controller is start running
  virtual bool isRunning() {
    return running_.load();
  }
  // Whether the Flow Controller has already been initialized (loaded flow XML)
  virtual bool isInitialized() {
    return initialized_.load();
  }
  // Start to run the Flow Controller which internally start the root process group and all its children
  virtual bool start();
  // Unload the current flow YAML, clean the root process group and all its children
  virtual void stop(bool force);
  // Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(const uint64_t timeToWaitMs);
  // Unload the current flow xml, clean the root process group and all its children
  virtual void unload();
  // Load new xml
  virtual void reload(std::string yamlFile);
  // update property value
  void updatePropertyValue(std::string processorName, std::string propertyName,
                           std::string propertyValue) {
    if (root_ != nullptr)
      root_->updatePropertyValue(processorName, propertyName, propertyValue);
  }

  // set 8 bytes SerialNumber
  virtual void setSerialNumber(uint8_t *number) {
    protocol_->setSerialNumber(number);
  }

  /**
   * Creates a controller service through the controller service provider impl.
   * @param type class name
   * @param id service identifier
   * @param firstTimeAdded first time this CS was added
   */
  virtual std::shared_ptr<core::controller::ControllerServiceNode> createControllerService(
      const std::string &type, const std::string &id,
      bool firstTimeAdded);

  /**
   * controller service provider
   */
  /**
   * removes controller service
   * @param serviceNode service node to be removed.
   */

  virtual void removeControllerService(
      const std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Enables the controller service services
   * @param serviceNode service node which will be disabled, along with linked services.
   */
  virtual void enableControllerService(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Enables controller services
   * @param serviceNoden vector of service nodes which will be enabled, along with linked services.
   */
  virtual void enableControllerServices(
      std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> serviceNodes);

  /**
   * Disables controller services
   * @param serviceNode service node which will be disabled, along with linked services.
   */
  virtual void disableControllerService(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Gets all controller services.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices();

  /**
   * Gets controller service node specified by <code>id</code>
   * @param id service identifier
   * @return shared pointer to the controller service node or nullptr if it does not exist.
   */
  virtual std::shared_ptr<core::controller::ControllerServiceNode> getControllerServiceNode(
      const std::string &id);

  virtual void verifyCanStopReferencingComponents(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Unschedules referencing components.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> unscheduleReferencingComponents(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Verify can disable referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual void verifyCanDisableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Disables referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> disableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Verify can enable referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual void verifyCanEnableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Determines if the controller service specified by identifier is enabled.
   */
  bool isControllerServiceEnabled(const std::string &identifier);

  /**
   * Enables referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> enableReferencingServices(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Schedules referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> scheduleReferencingComponents(
      std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Returns controller service components referenced by serviceIdentifier from the embedded
   * controller service provider;
   */
  std::shared_ptr<core::controller::ControllerService> getControllerServiceForComponent(
      const std::string &serviceIdentifier, const std::string &componentId);

  /**
   * Enables all controller services for the provider.
   */
  virtual void enableAllControllerServices();

 protected:

  // function to load the flow file repo.
  void loadFlowRepo();

  /**
   * Initializes flow controller paths.
   */
  virtual void initializePaths(const std::string &adjustedFilename);

  // flow controller mutex
  std::recursive_mutex mutex_;

  // Configuration File Name
  std::string configuration_file_name_;
  // NiFi property File Name
  std::string properties_file_name_;
  // Root Process Group
  std::shared_ptr<core::ProcessGroup> root_;
  // MAX Timer Driven Threads
  int max_timer_driven_threads_;
  // MAX Event Driven Threads
  int max_event_driven_threads_;
  // FlowFile Repo
  // Whether it is running
  std::atomic<bool> running_;
  // conifiguration filename
  std::string configuration_filename_;
  // Whether it has already been initialized (load the flow XML already)
  std::atomic<bool> initialized_;
  // Provenance Repo
  std::shared_ptr<core::Repository> provenance_repo_;

  // FlowFile Repo
  std::shared_ptr<core::Repository> flow_file_repo_;

  // Flow Engines
  // Flow Timer Scheduler
  std::shared_ptr<TimerDrivenSchedulingAgent> timer_scheduler_;
  // Flow Event Scheduler
  std::shared_ptr<EventDrivenSchedulingAgent> event_scheduler_;
  // Controller Service
  // Config
  // Site to Site Server Listener
  // Heart Beat
  // FlowControl Protocol
  FlowControlProtocol *protocol_;

  std::shared_ptr<Configure> configuration_;

  std::shared_ptr<core::controller::ControllerServiceMap> controller_service_map_;

  std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider_;
  // flow configuration object.
  std::unique_ptr<core::FlowConfiguration> flow_configuration_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
