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

#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ForwardingControllerServiceProvider.h"
#include "core/FlowConfiguration.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessGroup.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/Property.h"
#include "core/Relationship.h"
#include "core/state/nodes/FlowInformation.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "core/state/UpdateController.h"
#include "c2/C2Agent.h"
#include "CronDrivenSchedulingAgent.h"
#include "EventDrivenSchedulingAgent.h"
#include "properties/Configure.h"
#include "TimerDrivenSchedulingAgent.h"
#include "utils/Id.h"
#include "utils/file/FileSystem.h"
#include "utils/file/AssetManager.h"
#include "core/state/MetricsPublisherStore.h"
#include "RootProcessGroupWrapper.h"
#include "c2/ControllerSocketProtocol.h"
#include "core/BulletinStore.h"

namespace org::apache::nifi::minifi {

namespace state {
class ProcessorController;
}  // namespace state

class FlowController : public core::controller::ForwardingControllerServiceProvider,  public state::StateMonitor {
 public:
  FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
                 std::shared_ptr<Configure> configure, std::shared_ptr<core::FlowConfiguration> flow_configuration,
                 std::shared_ptr<core::ContentRepository> content_repo, std::unique_ptr<state::MetricsPublisherStore> metrics_publisher_store = nullptr,
                 std::shared_ptr<utils::file::FileSystem> filesystem = std::make_shared<utils::file::FileSystem>(), std::function<void()> request_restart = []{},
                 utils::file::AssetManager* asset_manager = {}, core::BulletinStore* bulletin_store = {});

  ~FlowController() override;

  virtual std::shared_ptr<core::Repository> getProvenanceRepository() {
    return this->provenance_repo_;
  }

  virtual void load(bool reload = false);

  void load(std::unique_ptr<core::ProcessGroup> root, bool reload = false) {
    root_wrapper_.setNewRoot(std::move(root));
    load(reload);
  }

  bool isRunning() const override {
    return running_.load() || updating_.isUpdating();
  }

  virtual bool isInitialized() {
    return initialized_.load();
  }
  // Start the Flow Controller which internally starts the root process group and all its children
  int16_t start() override;
  int16_t pause() override;
  int16_t resume() override;
  // Unload the current flow, clean the root process group and all its children
  int16_t stop() override;
  nonstd::expected<void, std::string> applyUpdate(const std::string &source, const std::string &configuration, bool persist, const std::optional<std::string>& flow_id) override;
  int16_t drainRepositories() override {
    return -1;
  }

  void executeOnComponent(const std::string& id_or_name, std::function<void(state::StateController&)> func) override;
  void executeOnAllComponents(std::function<void(state::StateController&)> func) override;

  int16_t clearConnection(const std::string &connection) override;

  std::vector<std::string> getSupportedConfigurationFormats() const override;

  // Asynchronous function trigger unloading and wait for a period of time
  virtual void waitUnload(const std::chrono::milliseconds time_to_wait);
  void updatePropertyValue(std::string processorName, std::string propertyName, std::string propertyValue) {
    root_wrapper_.updatePropertyValue(std::move(processorName), std::move(propertyName), std::move(propertyValue));
  }

  // validate and apply passing configuration payload
  // first it will validate the payload with the current root node config for flowController
  // like FlowController id/name is the same and new version is greater than the current version
  // after that, it will apply the configuration
  nonstd::expected<void, std::string> applyConfiguration(const std::string &source, const std::string &configurePayload, const std::optional<std::string>& flow_id = std::nullopt);

  std::string getName() const override {
    return root_wrapper_.getName();
  }

  std::string getComponentName() const override {
    return "FlowController";
  }

  utils::Identifier getComponentUUID() const override {
    return root_wrapper_.getComponentUUID();
  }

  virtual std::string getVersion() {
    return root_wrapper_.getVersion();
  }

  uint64_t getUptime() override;

  std::vector<BackTrace> getTraces() override;

  std::map<std::string, std::unique_ptr<io::InputStream>> getDebugInfo() override;

 private:
  class UpdateState {
   public:
    bool isUpdating() const { return update_block_count_ > 0; }
    void beginUpdate() { ++update_block_count_; }
    void endUpdate() { --update_block_count_; }
    void lock() { beginUpdate(); }
    void unlock() { endUpdate(); }

   private:
    std::atomic<uint32_t> update_block_count_;
  };

  /**
   * Loads the flow as specified in the flow config file or if not present
   * tries to fetch it from the C2 server (if enabled).
   * @return the built flow
   */
  std::unique_ptr<core::ProcessGroup> loadInitialFlow();

  void loadFlowRepo();
  std::vector<state::StateController*> getAllComponents();
  state::StateController* getComponent(const std::string& id_or_name);
  gsl::not_null<std::unique_ptr<state::ProcessorController>> createController(core::Processor& processor);
  std::unique_ptr<core::ProcessGroup> updateFromPayload(const std::string& url, const std::string& config_payload, const std::optional<std::string>& flow_id = std::nullopt);

  template <typename T, typename = typename std::enable_if<std::is_base_of<SchedulingAgent, T>::value>::type>
  void conditionalReloadScheduler(std::unique_ptr<T>& scheduler, const bool condition) {
    if (condition) {
      scheduler = std::make_unique<T>(gsl::not_null<core::controller::ControllerServiceProvider*>(this), provenance_repo_, flow_file_repo_, content_repo_, configuration_, thread_pool_);
    }
  }

  std::recursive_mutex mutex_;
  std::atomic<bool> running_;
  UpdateState updating_;
  std::atomic<bool> initialized_;
  std::unique_ptr<TimerDrivenSchedulingAgent> timer_scheduler_;
  std::unique_ptr<EventDrivenSchedulingAgent> event_scheduler_;
  std::unique_ptr<CronDrivenSchedulingAgent> cron_scheduler_;
  std::chrono::steady_clock::time_point start_time_;
  // Thread pool for schedulers
  utils::ThreadPool thread_pool_;
  std::shared_ptr<Configure> configuration_;
  std::shared_ptr<core::Repository> provenance_repo_;
  std::shared_ptr<core::Repository> flow_file_repo_;
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<core::FlowConfiguration> flow_configuration_;
  std::unique_ptr<state::MetricsPublisherStore> metrics_publisher_store_;
  RootProcessGroupWrapper root_wrapper_;
  std::unique_ptr<c2::C2Agent> c2_agent_{};
  std::unique_ptr<c2::ControllerSocketProtocol> controller_socket_protocol_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FlowController>::getLogger();
};

}  // namespace org::apache::nifi::minifi
