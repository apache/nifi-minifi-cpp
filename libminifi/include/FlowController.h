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
#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
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
#include "core/controller/ForwardingControllerServiceProvider.h"
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
#include "c2/C2Client.h"
#include "CronDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "FlowControlProtocol.h"
#include "FlowFileRecord.h"
#include "properties/Configure.h"
#include "TimerDrivenSchedulingAgent.h"
#include "utils/Id.h"
#include "utils/file/FileSystem.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "core/state/MetricsPublisher.h"

namespace org::apache::nifi::minifi {

namespace state {
class ProcessorController;
}  // namespace state

// Default NiFi Root Group Name
#define DEFAULT_ROOT_GROUP_NAME ""

/**
 * Flow Controller class. Generally used by FlowController factory
 * as a singleton.
 */
class FlowController : public core::controller::ForwardingControllerServiceProvider,  public state::StateMonitor, public c2::C2Client {
 public:
  FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
                 std::shared_ptr<Configure> configure, std::unique_ptr<core::FlowConfiguration> flow_configuration,
                 std::shared_ptr<core::ContentRepository> content_repo, const std::string& name = DEFAULT_ROOT_GROUP_NAME,
                 std::shared_ptr<utils::file::FileSystem> filesystem = std::make_shared<utils::file::FileSystem>(),
                 std::function<void()> request_restart = []{});

  FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
                 std::shared_ptr<Configure> configure, std::unique_ptr<core::FlowConfiguration> flow_configuration,
                 std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<utils::file::FileSystem> filesystem,
                 std::function<void()> request_restart = []{});

  ~FlowController() override;

  // Get the provenance repository
  virtual std::shared_ptr<core::Repository> getProvenanceRepository() {
    return this->provenance_repo_;
  }

  // Load flow xml from disk, after that, create the root process group and its children, initialize the flows
  virtual void load(std::unique_ptr<core::ProcessGroup> root = nullptr, bool reload = false);

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
  int16_t pause() override;
  int16_t resume() override;
  // Unload the current flow YAML, clean the root process group and all its children
  int16_t stop() override;
  int16_t applyUpdate(const std::string &source, const std::string &configuration, bool persist, const std::optional<std::string>& flow_id) override;
  int16_t drainRepositories() override {
    return -1;
  }

  void executeOnComponent(const std::string& id_or_name, std::function<void(state::StateController&)> func) override;
  void executeOnAllComponents(std::function<void(state::StateController&)> func) override;

  int16_t clearConnection(const std::string &connection) override;

  int16_t applyUpdate(const std::string& /*source*/, const std::shared_ptr<state::Update>&) override { return -1; }
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
  bool applyConfiguration(const std::string &source, const std::string &configurePayload, const std::optional<std::string>& flow_id = std::nullopt);

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

  utils::Identifier getComponentUUID() const override {
    if (!root_) {
      return {};
    }
    return root_->getUUID();
  }

  // get version
  virtual std::string getVersion() {
    if (root_ != nullptr)
      return std::to_string(root_->getVersion());
    else
      return "0";
  }

  utils::Identifier getControllerUUID() const override {
    return getUUID();
  }

  /**
   * Retrieves the agent manifest to be sent as a response to C2 DESCRIBE manifest
   * @return the agent manifest response node
   */
  state::response::NodeReporter::ReportedNode getAgentManifest() override;

  uint64_t getUptime() override;

  std::vector<BackTrace> getTraces() override;

  std::map<std::string, std::unique_ptr<io::InputStream>> getDebugInfo() override;

 private:
  /**
   * Loads the flow as specified in the flow config file or if not present
   * tries to fetch it from the C2 server (if enabled).
   * @return the built flow
   */
  std::unique_ptr<core::ProcessGroup> loadInitialFlow();

  void loadMetricsPublisher();

 protected:
  // function to load the flow file repo.
  void loadFlowRepo();

  std::optional<std::chrono::milliseconds> loadShutdownTimeoutFromConfiguration();

 private:
  template <typename T, typename = typename std::enable_if<std::is_base_of<SchedulingAgent, T>::value>::type>
  void conditionalReloadScheduler(std::shared_ptr<T>& scheduler, const bool condition) {
    if (condition) {
      scheduler = std::make_shared<T>(gsl::not_null<core::controller::ControllerServiceProvider*>(this), provenance_repo_, flow_file_repo_, content_repo_, configuration_, thread_pool_);
    }
  }

 protected:
  // flow controller mutex
  std::recursive_mutex mutex_;

  // Whether it is running
  std::atomic<bool> running_;
  std::atomic<bool> updating_;

  // Whether it has already been initialized (load the flow XML already)
  std::atomic<bool> initialized_;
  // Flow Timer Scheduler
  std::shared_ptr<TimerDrivenSchedulingAgent> timer_scheduler_;
  // Flow Event Scheduler
  std::shared_ptr<EventDrivenSchedulingAgent> event_scheduler_;
  // Cron Schedule
  std::shared_ptr<CronDrivenSchedulingAgent> cron_scheduler_;
  // FlowControl Protocol
  std::unique_ptr<FlowControlProtocol> protocol_;
  // metrics information
  std::chrono::steady_clock::time_point start_time_;

 private:
  std::vector<state::StateController*> getAllComponents();

  state::StateController* getComponent(const std::string& id_or_name);

  state::StateController* getProcessorController(const std::string& id_or_name,
      const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory);

  std::vector<state::StateController*> getAllProcessorControllers(
          const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory);

  gsl::not_null<std::unique_ptr<state::ProcessorController>> createController(core::Processor& processor);

  std::chrono::milliseconds shutdown_check_interval_{1000};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FlowController>::getLogger();
  std::string serial_number_;

  // Thread pool for schedulers
  utils::ThreadPool<utils::TaskRescheduleInfo> thread_pool_;
  std::map<utils::Identifier, std::unique_ptr<state::ProcessorController>> processor_to_controller_;
  std::unique_ptr<state::MetricsPublisher> metrics_publisher_;
};

}  // namespace org::apache::nifi::minifi
