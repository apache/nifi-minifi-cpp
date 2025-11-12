/**
 * @file FlowController.cpp
 * FlowController class implementation
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
#include <vector>
#include <map>
#include <chrono>
#include <future>
#include <thread>
#include <utility>
#include <memory>
#include <string>

#include "FlowController.h"
#include "core/state/ProcessorController.h"
#include "core/ProcessGroup.h"
#include "core/Core.h"
#include "SchedulingAgent.h"
#include "core/controller/ForwardingControllerServiceProvider.h"
#include "core/logging/LoggerConfiguration.h"
#include "minifi-cpp/core/Connectable.h"
#include "utils/file/PathUtils.h"
#include "utils/file/FileSystem.h"
#include "http/BaseHTTPClient.h"
#include "io/FileStream.h"
#include "core/ClassLoader.h"
#include "minifi-cpp/core/ThreadedRepository.h"
#include "c2/C2MetricsPublisher.h"
#include "c2/ControllerSocketMetricsPublisher.h"

namespace org::apache::nifi::minifi {

FlowController::FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
                               std::shared_ptr<Configure> configure, std::shared_ptr<core::FlowConfiguration> flow_configuration,
                               std::shared_ptr<core::ContentRepository> content_repo, std::unique_ptr<state::MetricsPublisherStore> metrics_publisher_store,
                               std::shared_ptr<utils::file::FileSystem> filesystem, std::function<void()> request_restart,
                               utils::file::AssetManager* asset_manager, core::BulletinStore* bulletin_store)
    : core::controller::ForwardingControllerServiceProvider(core::className<FlowController>()),
      running_(false),
      initialized_(false),
      thread_pool_(5, nullptr, "Flowcontroller threadpool"),
      configuration_(std::move(configure)),
      provenance_repo_(std::move(provenance_repo)),
      flow_file_repo_(std::move(flow_file_repo)),
      content_repo_(std::move(content_repo)),
      flow_configuration_(std::move(flow_configuration)),
      metrics_publisher_store_(std::move(metrics_publisher_store)),
      root_wrapper_(configuration_, metrics_publisher_store_.get()) {
  if (provenance_repo_ == nullptr)
    throw std::runtime_error("Provenance Repo should not be null");
  if (flow_file_repo_ == nullptr)
    throw std::runtime_error("Flow Repo should not be null");
  if (configuration_ == nullptr) {
    throw std::runtime_error("Must supply a configuration.");
  }

  if (flow_configuration_) {
    controller_service_provider_impl_ = flow_configuration_->getControllerServiceProvider();
  }
  if (metrics_publisher_store_) {
    metrics_publisher_store_->initialize(this, this);
  }

  if (c2::isC2Enabled(configuration_)) {
    std::shared_ptr<c2::C2MetricsPublisher> c2_metrics_publisher;
    if (auto publisher = metrics_publisher_store_->getMetricsPublisher(c2::C2_METRICS_PUBLISHER).lock()) {
      c2_metrics_publisher = std::dynamic_pointer_cast<c2::C2MetricsPublisher>(publisher);
    }
    c2_agent_ = std::make_unique<c2::C2Agent>(configuration_, c2_metrics_publisher, std::move(filesystem), std::move(request_restart), asset_manager);
  }

  if (c2::isControllerSocketEnabled(configuration_)) {
    std::shared_ptr<c2::ControllerSocketMetricsPublisher> controller_socket_metrics_publisher;
    if (auto publisher = metrics_publisher_store_->getMetricsPublisher(c2::CONTROLLER_SOCKET_METRICS_PUBLISHER).lock()) {
      controller_socket_metrics_publisher = std::dynamic_pointer_cast<c2::ControllerSocketMetricsPublisher>(publisher);
      controller_socket_metrics_publisher->setFlowStatusDependencies(bulletin_store, flow_file_repo_->getDirectory(), content_repo_->getStoragePath());
    }
    controller_socket_protocol_ = std::make_unique<c2::ControllerSocketProtocol>(*this, configuration_, controller_socket_metrics_publisher);
    root_wrapper_.setControllerSocketProtocol(controller_socket_protocol_.get());
  }
}

FlowController::~FlowController() {
  if (c2_agent_) {
    c2_agent_->stop();
  }
  stop();
  // TODO(adebreceni): are these here on purpose, so they are destroyed first?
  flow_file_repo_ = nullptr;
  provenance_repo_ = nullptr;
  logger_->log_trace("Destroying FlowController");
}

nonstd::expected<void, std::string> FlowController::applyConfiguration(const std::string &source, const std::string &configurePayload, const std::optional<std::string>& flow_id) {
  std::unique_ptr<core::ProcessGroup> newRoot;
  try {
    newRoot = updateFromPayload(source, configurePayload, flow_id);
  } catch (const std::exception& ex) {
    logger_->log_error("Invalid configuration payload, type: {}, what: {}", typeid(ex).name(), ex.what());
    return nonstd::make_unexpected(fmt::format("Invalid configuration payload, type: {}, what: {}", typeid(ex).name(), ex.what()));
  } catch (...) {
    logger_->log_error("Invalid configuration payload, type: {}", getCurrentExceptionTypeName());
    return nonstd::make_unexpected(fmt::format("Invalid configuration payload, type: {}", getCurrentExceptionTypeName()));
  }

  if (newRoot == nullptr)
    return nonstd::make_unexpected("Could not create root process group");

  logger_->log_info("Starting to reload Flow Controller with flow control name {}, version {}", newRoot->getName(), newRoot->getVersion());

  bool started = false;
  {
    std::scoped_lock<UpdateState> update_lock(updating_);
    std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
    stop();

    root_wrapper_.setNewRoot(std::move(newRoot));
    initialized_ = false;
    try {
      load(true);
      started = start() == 0;
    } catch (const std::exception& ex) {
      logger_->log_error("Caught exception while starting flow, type {}, what: {}", typeid(ex).name(), ex.what());
    } catch (...) {
      logger_->log_error("Caught unknown exception while starting flow, type {}", getCurrentExceptionTypeName());
    }
    if (!started) {
      logger_->log_error("Failed to start new flow, restarting previous flow");
      root_wrapper_.restoreBackup();
      load(true);
      start();
    } else {
      root_wrapper_.clearBackup();
    }
  }

  if (started) {
    auto flowVersion = flow_configuration_->getFlowVersion();
    if (flowVersion) {
      logger_->log_debug("Setting flow id to {}", flowVersion->getFlowId());
      logger_->log_debug("Setting flow url to {}", flowVersion->getFlowIdentifier()->getRegistryUrl());
      configuration_->set(Configure::nifi_c2_flow_id, flowVersion->getFlowId());
      configuration_->set(Configure::nifi_c2_flow_url, flowVersion->getFlowIdentifier()->getRegistryUrl());
    } else {
      logger_->log_debug("Invalid flow version, not setting");
    }
  }

  if (!started) {
    return nonstd::make_unexpected("Failed to start flow");
  }
  return {};
}

int16_t FlowController::stop() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    // immediately indicate that we are not running
    logger_->log_info("Stop Flow Controller");
    root_wrapper_.stopProcessing(*timer_scheduler_, *event_scheduler_, *cron_scheduler_);
    // stop after we've attempted to stop the processors.
    timer_scheduler_->stop();
    event_scheduler_->stop();
    cron_scheduler_->stop();
    thread_pool_.shutdown();
    /* STOP! Before you change it, consider the following:
     * -Stopping the schedulers doesn't actually quit the onTrigger functions of processors
     * -They only guarantee that the processors are not scheduled anymore
     * -After the threadpool is stopped we can make sure that processors don't need repos and controllers anymore */
    root_wrapper_.drainConnections();
    this->flow_file_repo_->stop();
    this->provenance_repo_->stop();
    this->content_repo_->stop();
    // stop the ControllerServices
    disableAllControllerServices();
    running_ = false;
  }
  return 0;
}

/**
 * This function will attempt to unload yaml and stop running Processors.
 *
 * If the latter attempt fails or does not complete within the prescribed
 * period, running_ will be set to false and we will return.
 *
 * @param timeToWaitMs Maximum time to wait before manually
 * marking running as false.
 */
void FlowController::waitUnload(const std::chrono::milliseconds time_to_wait) {
  if (running_) {
    // use the current time and increment with the provided argument.
    std::chrono::system_clock::time_point wait_time = std::chrono::system_clock::now() + time_to_wait;

    // create an asynchronous future.
    std::future<void> unload_task = std::async(std::launch::async, [this]() {stop();});

    if (std::future_status::ready == unload_task.wait_until(wait_time)) {
      running_ = false;
    }
  }
}

std::unique_ptr<core::ProcessGroup> FlowController::loadInitialFlow() {
  std::unique_ptr<core::ProcessGroup> root = flow_configuration_->getRoot();
  if (root) {
    return root;
  }
  logger_->log_error("Couldn't load flow configuration file, trying to fetch it from C2 server");
  auto opt_flow_url = configuration_->get(Configure::nifi_c2_flow_url);
  if (!opt_flow_url) {
    logger_->log_error("No flow configuration url found");
    return nullptr;
  }
  // ensure that C2 connection is up and running
  // since we don't have access to the flow definition, the C2 communication
  // won't be able to use the services defined there, e.g. SSLContextService
  if (!c2_agent_) {
    return nullptr;
  }
  c2_agent_->initialize(this, this, this);
  c2_agent_->start();
  auto opt_source = c2_agent_->fetchFlow(*opt_flow_url);
  if (!opt_source) {
    logger_->log_error("Couldn't fetch flow configuration from C2 server");
    return nullptr;
  }
  root = updateFromPayload(*opt_flow_url, *opt_source);
  if (root) {
    logger_->log_info("Successfully fetched valid flow configuration");
    if (!flow_configuration_->persist(*root)) {
      logger_->log_info("Failed to write the fetched flow to disk");
    }
  }
  return root;
}

void FlowController::load(bool reload) {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    stop();
  }

  {
    std::scoped_lock<UpdateState> update_lock(updating_);
    if (!root_wrapper_.initialized()) {
      logger_->log_info("Instantiating new flow");
      root_wrapper_.setNewRoot(loadInitialFlow());
      if (c2_agent_ && !c2_agent_->isControllerRunning()) {
        // TODO(lordgamez): this initialization configures the C2 sender protocol (e.g. RESTSender) which may contain an SSL Context service from the flow config
        // for SSL communication. This service may change on flow update and we should take care of the SSL Context Service change in the C2 Agent.
        c2_agent_->initialize(this, this, this);
        c2_agent_->start();
      }
    }
  }

  logger_->log_info("Loaded root processor Group");
  logger_->log_info("Initializing timers");
  if (!thread_pool_.isRunning() || reload) {
    thread_pool_.shutdown();
    thread_pool_.setMaxConcurrentTasks(configuration_->getInt(Configure::nifi_flow_engine_threads, 5));
    thread_pool_.setControllerServiceProvider([this] (std::string_view name) -> std::shared_ptr<core::controller::ControllerServiceInterface> {
      if (auto service = this->getControllerService(std::string{name})) {
        return {service, service->getImplementation()};
      }
      return {};
    });
    thread_pool_.start();
  }

  conditionalReloadScheduler<TimerDrivenSchedulingAgent>(timer_scheduler_, !timer_scheduler_ || reload);
  conditionalReloadScheduler<EventDrivenSchedulingAgent>(event_scheduler_, !event_scheduler_ || reload);
  conditionalReloadScheduler<CronDrivenSchedulingAgent>(cron_scheduler_, !cron_scheduler_ || reload);

  logger_->log_info("Loaded controller service provider");

  /*
    * Without reset we have to distinguish a fresh restart and a reload, to decide if we have to
    * increment the claims' counter on behalf of the persisted instances.
    * ResourceClaim::getStreamCount is not suitable as multiple persisted instances
    * might have the same claim.
    * e.g. without reset a streamCount of 3 could mean the following:
    *  - it was a fresh restart and 3 instances of this claim have already been resurrected -> we must increment
    *  - it was a reload and 3 instances have been persisted before the shutdown -> we must not increment
    */
  content_repo_->reset();
  logger_->log_info("Reset content repository");

  // Load Flow File from Repo
  loadFlowRepo();
  logger_->log_info("Loaded flow repository");
  initialized_ = true;
}

void FlowController::loadFlowRepo() {
  if (this->flow_file_repo_ != nullptr) {
    logger_->log_debug("Getting connection map");
    std::map<std::string, core::Connectable*> connectionMap;
    std::map<std::string, core::Connectable*> containers;
    root_wrapper_.getConnections(connectionMap);
    root_wrapper_.getFlowFileContainers(containers);
    flow_file_repo_->setConnectionMap(connectionMap);
    flow_file_repo_->setContainers(containers);
    flow_file_repo_->loadComponent(content_repo_);
  } else {
    logger_->log_debug("Flow file repository is not set");
  }
}

int16_t FlowController::start() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (!initialized_) {
    logger_->log_error("Can not start Flow Controller because it has not been initialized");
    return -1;
  } else if (!running_) {
    logger_->log_info("Starting Flow Controller");
    enableAllControllerServices();
    if (controller_socket_protocol_) {
      // Initialization is postponed after controller services are enabled so the controller socket may load the SSL context defined in the flow configuration
      controller_socket_protocol_->initialize();
    }
    timer_scheduler_->start();
    event_scheduler_->start();
    cron_scheduler_->start();

    // watch out, this might immediately start the processors
    // as the thread_pool_ is started in load()
    if (root_wrapper_.startProcessing(*timer_scheduler_, *event_scheduler_, *cron_scheduler_)) {
      start_time_ = std::chrono::steady_clock::now();
    }

    core::logging::LoggerConfiguration::getConfiguration().initializeAlertSinks(configuration_);
    running_ = true;
    content_repo_->start();
    provenance_repo_->start();
    flow_file_repo_->start();
    thread_pool_.start();
    logger_->log_info("Started Flow Controller");
  }
  return 0;
}

int16_t FlowController::pause() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (!running_) {
    logger_->log_warn("Can not pause flow controller that is not running");
    return 0;
  }

  logger_->log_info("Pausing Flow Controller");
  thread_pool_.pause();
  return 0;
}

int16_t FlowController::resume() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (!running_) {
    logger_->log_warn("Can not resume flow controller tasks because the flow controller is not running");
    return 0;
  }

  logger_->log_info("Resuming Flow Controller");
  thread_pool_.resume();
  return 0;
}

std::vector<std::string> FlowController::getSupportedConfigurationFormats() const {
  return flow_configuration_->getSupportedFormats();
}

nonstd::expected<void, std::string> FlowController::applyUpdate(const std::string &source, const std::string &configuration, bool persist, const std::optional<std::string>& flow_id) {
  auto result = applyConfiguration(source, configuration, flow_id);
  if (result) {
    if (persist) {
      const auto* process_group = root_wrapper_.getRoot();
      gsl_Expects(process_group);
      flow_configuration_->persist(*process_group);
    }
  }
  return result;
}

int16_t FlowController::clearConnection(const std::string &connection) {
  root_wrapper_.clearConnection(connection);
  return -1;
}

void FlowController::executeOnAllComponents(std::function<void(state::StateController&)> func) {
  if (updating_.isUpdating()) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto* component: getAllComponents()) {
    func(*component);
  }
}

void FlowController::executeOnComponent(const std::string &id_or_name, std::function<void(state::StateController&)> func) {
  if (updating_.isUpdating()) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (auto* component = getComponent(id_or_name); component != nullptr) {
    func(*component);
  } else {
    logger_->log_error("Could not get execute requested callback for component \"{}\", because component was not found", id_or_name);
  }
}

std::vector<state::StateController*> FlowController::getAllComponents() {
  if (auto components = root_wrapper_.getAllProcessorControllers([this](core::Processor& p) { return createController(p); })) {
    components->push_back(this);
    return *components;
  }

  return {this};
}

state::StateController* FlowController::getComponent(const std::string& id_or_name) {
  if (id_or_name == getUUIDStr() || id_or_name == "FlowController") {
    return this;
  } else if (auto controller = root_wrapper_.getProcessorController(id_or_name, [this](core::Processor& p) { return createController(p); })) {
    return controller;
  }

  return nullptr;
}

gsl::not_null<std::unique_ptr<state::ProcessorController>> FlowController::createController(core::Processor& processor) {
  const auto scheduler = [this, &processor]() -> SchedulingAgent& {
    switch (processor.getSchedulingStrategy()) {
      case core::SchedulingStrategy::TIMER_DRIVEN: return *timer_scheduler_;
      case core::SchedulingStrategy::EVENT_DRIVEN: return *event_scheduler_;
      case core::SchedulingStrategy::CRON_DRIVEN: return *cron_scheduler_;
    }
    gsl_Assert(false);
  };
  return gsl::make_not_null(std::make_unique<state::ProcessorController>(processor, scheduler()));
}

uint64_t FlowController::getUptime() {
  auto now = std::chrono::steady_clock::now();
  auto time_since = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
  return time_since;
}

std::vector<BackTrace> FlowController::getTraces() {
  std::vector<BackTrace> traces{thread_pool_.getTraces()};
  if (auto provenance_repo = std::dynamic_pointer_cast<core::ThreadedRepository>(provenance_repo_)) {
    auto prov_repo_trace = provenance_repo->getTraces();
    traces.emplace_back(std::move(prov_repo_trace));
  }
  if (auto flow_file_repo = std::dynamic_pointer_cast<core::ThreadedRepository>(flow_file_repo_)) {
    auto flow_repo_trace = flow_file_repo->getTraces();
    traces.emplace_back(std::move(flow_repo_trace));
  }
  auto my_traces = TraceResolver::getResolver().getBackTrace("main");
  traces.emplace_back(std::move(my_traces));
  return traces;
}

std::map<std::string, std::unique_ptr<io::InputStream>> FlowController::getDebugInfo() {
  std::map<std::string, std::unique_ptr<io::InputStream>> debug_info;
  auto logs = core::logging::LoggerConfiguration::getCompressedLogs();
  for (size_t i = 0; i < logs.size(); ++i) {
    std::string index_str = i == logs.size() - 1 ? "" : "." + std::to_string(logs.size() - 1 - i);
    debug_info["minifi.log" + index_str + ".gz"] = std::move(logs[i]);
  }
  if (auto opt_flow_path = flow_configuration_->getConfigurationPath()) {
    debug_info["config.yml"] = std::make_unique<io::FileStream>(opt_flow_path.value(), 0, false);
  }
  debug_info["minifi.properties"] = std::make_unique<io::FileStream>(configuration_->getFilePath(), 0, false);

  return debug_info;
}

std::unique_ptr<core::ProcessGroup> FlowController::updateFromPayload(const std::string& url, const std::string& config_payload, const std::optional<std::string>& flow_id) {
  auto root = flow_configuration_->updateFromPayload(url, config_payload, flow_id);
  // prepare to accept the new controller service provider from flow_configuration_
  clearControllerServices();
  controller_service_provider_impl_ = flow_configuration_->getControllerServiceProvider();
  return root;
}

}  // namespace org::apache::nifi::minifi
