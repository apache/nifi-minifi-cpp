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
#ifndef LIBMINIFI_INCLUDE_FLOWCONTROLLER_H_
#define LIBMINIFI_INCLUDE_FLOWCONTROLLER_H_

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <cstdio>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Connection.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/FlowConfiguration.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/ProcessGroup.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/Relationship.h"
#include "core/state/nodes/FlowInformation.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/state/UpdateController.h"
#include "CronDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"
#include "FlowFileRecord.h"
#include "properties/Configure.h"
#include "TimerDrivenSchedulingAgent.h"
#include "utils/Id.h"

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
class FlowController : public core::controller::ControllerServiceProvider, public state::response::NodeReporter,  public state::StateMonitor, public std::enable_shared_from_this<FlowController> {
 public:
  /**
   * Flow controller constructor
   */
  explicit FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                          std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<core::ContentRepository> content_repo, std::string name, bool headless_mode);

  explicit FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                          std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<core::ContentRepository> content_repo)
      : FlowController(std::move(provenance_repo), std::move(flow_file_repo), std::move(configure), std::move(flow_configuration), std::move(content_repo), DEFAULT_ROOT_GROUP_NAME, false) {
  }

  explicit FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                          std::unique_ptr<core::FlowConfiguration> flow_configuration)
      : FlowController(std::move(provenance_repo), std::move(flow_file_repo), std::move(configure), std::move(flow_configuration),
          std::make_shared<core::repository::FileSystemRepository>(), DEFAULT_ROOT_GROUP_NAME, false) {
    content_repo_->initialize(configuration_);
  }

  // Destructor
  ~FlowController() override;

  // Get the provenance repository
  virtual std::shared_ptr<core::Repository> getProvenanceRepository() {
    return this->provenance_repo_;
  }

  // Get the flowfile repository
  virtual std::shared_ptr<core::Repository> getFlowFileRepository() {
    return this->flow_file_repo_;
  }

  // Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  // virtual void load(const std::shared_ptr<core::ProcessGroup> &root = nullptr, bool reload = false);
  virtual void load_without_reload(const std::shared_ptr<core::ProcessGroup> &root = nullptr);
  virtual void load_with_reload(const std::shared_ptr<core::ProcessGroup> &root = nullptr);

  // Whether the Flow Controller is start running
  bool isRunning() override {
    return running_.load() || updating_.load();
  }

  // Whether the Flow Controller has already been initialized (loaded flow XML)
  virtual bool isInitialized() {
    return initialized_.load();
  }
  // Start to run the Flow Controller which internally start the root process group and all its children
  int16_t start() override;
  int16_t pause() override {
    return -1;
  }
  // Unload the current flow YAML, clean the root process group and all its children
  int16_t stop() override;
  int16_t applyUpdate(const std::string &source, const std::string &configuration) override;
  int16_t drainRepositories() override {
    return -1;
  }

  std::vector<std::shared_ptr<state::StateController>> getComponents(const std::string &name) override;

  std::vector<std::shared_ptr<state::StateController>> getAllComponents() override;

  int16_t clearConnection(const std::string &connection) override;

  int16_t applyUpdate(const std::string &source, const std::shared_ptr<state::Update>&) override { return -1; }
  // Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(uint64_t timeToWaitMs);
  // Unload the current flow xml, clean the root process group and all its children
  virtual void unload();
  // update property value
  void updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue) {
    if (root_ != nullptr)
      root_->updatePropertyValue(std::move(processorName), std::move(propertyName), std::move(propertyValue));
  }

  // set SerialNumber
  void setSerialNumber(std::string number) {
    serial_number_ = std::move(number);
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
  std::string getName() const override {
    if (root_ != nullptr)
      return root_->getName();
    else
      return "";
  }

  std::string getComponentName() const override {
    return "FlowController";
  }

  std::string getComponentUUID() const override {
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
  std::shared_ptr<core::controller::ControllerServiceNode> createControllerService(const std::string &type, const std::string &fullType, const std::string &id, bool firstTimeAdded) override;

  /**
   * controller service provider
   */
  /**
   * removes controller service
   * @param serviceNode service node to be removed.
   */

  void removeControllerService(const std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Enables the controller service services
   * @param serviceNode service node which will be disabled, along with linked services.
   */
  std::future<utils::TaskRescheduleInfo> enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Enables controller services
   * @param serviceNoden vector of service nodes which will be enabled, along with linked services.
   */
  void enableControllerServices(std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> serviceNodes) override;

  /**
   * Disables controller services
   * @param serviceNode service node which will be disabled, along with linked services.
   */
  std::future<utils::TaskRescheduleInfo> disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Removes all controller services.
   */
  void clearControllerServices() override;

  /**
   * Gets all controller services.
   */
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> getAllControllerServices() override;

  std::shared_ptr<core::controller::ControllerService> getControllerService(const std::string &identifier) override;

  /**
   * Gets controller service node specified by <code>id</code>
   * @param id service identifier
   * @return shared pointer to the controller service node or nullptr if it does not exist.
   */
  std::shared_ptr<core::controller::ControllerServiceNode> getControllerServiceNode(const std::string &id) const override;

  void verifyCanStopReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Unschedules referencing components.
   */
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> unscheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Verify can disable referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  void verifyCanDisableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Disables referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> disableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Verify can enable referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  void verifyCanEnableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode>&) override;

  /**
   * Determines if the controller service specified by identifier is enabled.
   */
  bool isControllerServiceEnabled(const std::string &identifier) override;

  /**
   * Enables referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> enableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Schedules referencing components
   * @param serviceNode service node whose referenced components will be scheduled.
   */
  std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> scheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) override;

  /**
   * Returns controller service components referenced by serviceIdentifier from the embedded
   * controller service provider;
   */
  std::shared_ptr<core::controller::ControllerService> getControllerServiceForComponent(const std::string &serviceIdentifier, const std::string &componentId) override;

  /**
   * Enables all controller services for the provider.
   */
  void enableAllControllerServices() override;

  /**
   * Disables all controller services for the provider.
   */
  void disableAllControllerServices() override;

  /**
   * Retrieves metrics node
   * @return metrics response node
   */
  std::shared_ptr<state::response::ResponseNode> getMetricsNode(const std::string& metricsClass) const override;

  /**
   * Retrieves root nodes configured to be included in heartbeat
   * @param includeManifest -- determines if manifest is to be included
   * @return a list of response nodes
   */
  std::vector<std::shared_ptr<state::response::ResponseNode>> getHeartbeatNodes(bool includeManifest) const override;

  /**
   * Retrieves the agent manifest to be sent as a response to C2 DESCRIBE manifest
   * @return the agent manifest response node
   */
  std::shared_ptr<state::response::ResponseNode> getAgentManifest() const override;

  uint64_t getUptime() override;

  std::vector<BackTrace> getTraces() override;

  void initializeC2();
  void stopC2();

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

  utils::optional<std::chrono::milliseconds> loadShutdownTimeoutFromConfiguration();

 private:
  void restartThreadPool();
  void initializeUninitializedSchedulers();

  template <typename T, typename = typename std::enable_if<std::is_base_of<SchedulingAgent, T>::value>::type>
  void conditionalReloadScheduler(std::shared_ptr<T>& scheduler, const bool condition) {
    if (condition) {
      scheduler = std::make_shared<T>(gsl::not_null<core::controller::ControllerServiceProvider*>(this), provenance_repo_, flow_file_repo_, content_repo_, configuration_, thread_pool_);
    }
  }

 protected:
  // flow controller mutex
  std::recursive_mutex mutex_;

  // Root Process Group
  std::shared_ptr<core::ProcessGroup> root_;
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
  // Thread pool for schedulers
  utils::ThreadPool<utils::TaskRescheduleInfo> thread_pool_;
  // Flow Timer Scheduler
  std::shared_ptr<TimerDrivenSchedulingAgent> timer_scheduler_;
  // Flow Event Scheduler
  std::shared_ptr<EventDrivenSchedulingAgent> event_scheduler_;
  // Cron Schedule
  std::shared_ptr<CronDrivenSchedulingAgent> cron_scheduler_;
  // FlowControl Protocol
  std::unique_ptr<FlowControlProtocol> protocol_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::controller::ControllerServiceMap> controller_service_map_;
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider_;
  // flow configuration object.
  std::unique_ptr<core::FlowConfiguration> flow_configuration_;
  // metrics information
  std::chrono::steady_clock::time_point start_time_;
  mutable std::mutex metrics_mutex_;
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
  std::chrono::milliseconds shutdown_check_interval_{1000};
  std::shared_ptr<logging::Logger> logger_;
  std::string serial_number_;
  std::unique_ptr<state::UpdateController> c2_agent_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_FLOWCONTROLLER_H_
