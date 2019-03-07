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
#include <stdio.h>
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
#include "core/state/nodes/MetricsBase.h"
#include "utils/Id.h"
#include "core/state/StateManager.h"
#include "core/state/nodes/FlowInformation.h"
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
class FlowController : public core::controller::ControllerServiceProvider, public state::StateManager {
 public:
  static const int DEFAULT_MAX_TIMER_DRIVEN_THREAD = 10;
  static const int DEFAULT_MAX_EVENT_DRIVEN_THREAD = 5;

  /**
   * Flow controller constructor
   */
  explicit FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                          std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<core::ContentRepository> content_repo, const std::string name, bool headless_mode);

  explicit FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                          std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<core::ContentRepository> content_repo)
      : FlowController(provenance_repo, flow_file_repo, configure, std::move(flow_configuration), content_repo, DEFAULT_ROOT_GROUP_NAME, false) {
  }

  explicit FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                          std::unique_ptr<core::FlowConfiguration> flow_configuration)
      : FlowController(provenance_repo, flow_file_repo, configure, std::move(flow_configuration), std::make_shared<core::repository::FileSystemRepository>(), DEFAULT_ROOT_GROUP_NAME, false) {
    content_repo_->initialize(configure);
  }

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
  virtual void load(const std::shared_ptr<core::ProcessGroup> &root = nullptr, bool reload = false);

  // Whether the Flow Controller is start running
  virtual bool isRunning() {
    return running_.load() || updating_.load();
  }

  // Whether the Flow Controller has already been initialized (loaded flow XML)
  virtual bool isInitialized() {
    return initialized_.load();
  }
  // Start to run the Flow Controller which internally start the root process group and all its children
  virtual int16_t start();
  virtual int16_t pause() {
    return -1;
  }
  // Unload the current flow YAML, clean the root process group and all its children
  virtual int16_t stop(bool force, uint64_t timeToWait = 0);
  virtual int16_t applyUpdate(const std::string &source, const std::string &configuration);
  virtual int16_t drainRepositories() {

    return -1;
  }

  virtual std::vector<std::shared_ptr<state::StateController>> getComponents(const std::string &name);

  virtual std::vector<std::shared_ptr<state::StateController>> getAllComponents();

  virtual int16_t clearConnection(const std::string &connection);

  virtual int16_t applyUpdate(const std::string &source, const std::shared_ptr<state::Update> &updateController) {
    return -1;
  }
  // Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(const uint64_t timeToWaitMs);
  // Unload the current flow xml, clean the root process group and all its children
  virtual void unload();
  // Load new xml
  virtual void reload(std::string yamlFile);
  // update property value
  void updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue) {
    if (root_ != nullptr)
      root_->updatePropertyValue(processorName, propertyName, propertyValue);
  }

  // set SerialNumber
  void setSerialNumber(std::string number) {
    serial_number_ = number;
  }

  // get serial number as string
  std::string getSerialNumber() {
    return serial_number_;
  }

  // validate and apply passing yaml configuration payload
  // first it will validate the payload with the current root node config for flowController
  // like FlowController id/name is the same and new version is greater than the current version
  // after that, it will apply the configuration
  bool applyConfiguration(const std::string &source, const std::string &configurePayload);

  // get name
  std::string getName() const {
    if (root_ != nullptr)
      return root_->getName();
    else
      return "";
  }

  virtual std::string getComponentName() const {
    return "FlowController";
  }

  virtual std::string getComponentUUID() const {
    utils::Identifier ident;
    root_->getUUID(ident);
    return ident.to_string();
  }

  // get version
  virtual std::string getVersion() {
    if (root_ != nullptr)
      return std::to_string(root_->getVersion());
    else
      return "0";
  }

  /**
   * Creates a controller service through the controller service provider impl.
   * @param type class name
   * @param id service identifier
   * @param firstTimeAdded first time this CS was added
   */
  virtual std::shared_ptr<core::controller::ControllerServiceNode> createControllerService(const std::string &type, const std::string &fullType, const std::string &id, bool firstTimeAdded);

  /**
   * controller service provider
   */
  /**
   * removes controller service
   * @param serviceNode service node to be removed.
   */

  virtual void removeControllerService(const std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Enables the controller service services
   * @param serviceNode service node which will be disabled, along with linked services.
   */
  virtual std::future<uint64_t> enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Enables controller services
   * @param serviceNoden vector of service nodes which will be enabled, along with linked services.
   */
  virtual void enableControllerServices(std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> serviceNodes);

  /**
   * Disables controller services
   * @param serviceNode service node which will be disabled, along with linked services.
   */
  virtual std::future<uint64_t> disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Gets all controller services.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices();

  virtual std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string &identifier);

  /**
   * Gets controller service node specified by <code>id</code>
   * @param id service identifier
   * @return shared pointer to the controller service node or nullptr if it does not exist.
   */
  virtual std::shared_ptr<core::controller::ControllerServiceNode> getControllerServiceNode(const std::string &id);

  virtual void verifyCanStopReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Unschedules referencing components.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> unscheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Verify can disable referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual void verifyCanDisableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Disables referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> disableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Verify can enable referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual void verifyCanEnableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Determines if the controller service specified by identifier is enabled.
   */
  bool isControllerServiceEnabled(const std::string &identifier);

  /**
   * Enables referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> enableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Schedules referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  virtual std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> scheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode);

  /**
   * Returns controller service components referenced by serviceIdentifier from the embedded
   * controller service provider;
   */
  std::shared_ptr<core::controller::ControllerService> getControllerServiceForComponent(const std::string &serviceIdentifier, const std::string &componentId);

  /**
   * Enables all controller services for the provider.
   */
  virtual void enableAllControllerServices();

  /**
   * Retrieves all root response nodes from this source.
   * @param metric_vector -- metrics will be placed in this vector.
   * @return result of the get operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t getResponseNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector, uint16_t metricsClass);
  /**
   * Retrieves all metrics from this source.
   * @param metric_vector -- metrics will be placed in this vector.
   * @return result of the get operation.
   *  0 Success
   *  1 No error condition, but cannot obtain lock in timely manner.
   *  -1 failure
   */
  virtual int16_t getMetricsNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector, uint16_t metricsClass);

  virtual uint64_t getUptime();

  virtual std::vector<BackTrace> getTraces();

  void initializeC2();

 protected:

  void loadC2ResponseConfiguration();

  void loadC2ResponseConfiguration(const std::string &prefix);

  std::shared_ptr<state::response::ResponseNode> loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode>);

  // function to load the flow file repo.
  void loadFlowRepo();

  void initializeExternalComponents();

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
  std::atomic<bool> updating_;

  // conifiguration filename
  std::string configuration_filename_;

  std::atomic<bool> c2_initialized_;
  std::atomic<bool> flow_update_;
  std::atomic<bool> c2_enabled_;
  // Whether it has already been initialized (load the flow XML already)
  std::atomic<bool> initialized_;
  // Provenance Repo
  std::shared_ptr<core::Repository> provenance_repo_;

  // FlowFile Repo
  std::shared_ptr<core::Repository> flow_file_repo_;

  std::shared_ptr<core::ContentRepository> content_repo_;

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

  // metrics information

  std::chrono::steady_clock::time_point start_time_;

  std::mutex metrics_mutex_;
  // root_nodes cache
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> root_response_nodes_;
  // metrics cache
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> device_information_;

  // metrics cache
  std::map<std::string, std::shared_ptr<state::response::ResponseNode>> component_metrics_;

  std::map<uint8_t, std::vector<std::shared_ptr<state::response::ResponseNode>>> component_metrics_by_id_;
  // metrics last run
  std::chrono::steady_clock::time_point last_metrics_capture_;

 private:
  std::shared_ptr<logging::Logger> logger_;
  std::string serial_number_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}
/* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
