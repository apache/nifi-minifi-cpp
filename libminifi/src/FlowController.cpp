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
#include "core/state/nodes/AgentInformation.h"
#include "core/state/ProcessorController.h"
#include "c2/C2Agent.h"
#include "core/ProcessGroup.h"
#include "core/Core.h"
#include "SchedulingAgent.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/controller/ForwardingControllerServiceProvider.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Connectable.h"
#include "utils/file/PathUtils.h"
#include "utils/file/FileSystem.h"
#include "utils/BaseHTTPClient.h"
#include "io/NetworkPrioritizer.h"
#include "io/FileStream.h"
#include "core/ClassLoader.h"
#include "core/ThreadedRepository.h"

namespace org::apache::nifi::minifi {

FlowController::FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
                               std::shared_ptr<Configure> configure, std::unique_ptr<core::FlowConfiguration> flow_configuration,
                               std::shared_ptr<core::ContentRepository> content_repo, const std::string& /*name*/,
                               std::shared_ptr<utils::file::FileSystem> filesystem, std::function<void()> request_restart)
    : core::controller::ForwardingControllerServiceProvider(core::getClassName<FlowController>()),
      c2::C2Client(std::move(configure), std::move(provenance_repo), std::move(flow_file_repo),
                   std::move(content_repo), std::move(flow_configuration), std::move(filesystem),
                   std::move(request_restart), core::logging::LoggerFactory<c2::C2Client>::getLogger()),
      running_(false),
      updating_(false),
      initialized_(false),
      thread_pool_(2, false, nullptr, "Flowcontroller threadpool") {
  if (provenance_repo_ == nullptr)
    throw std::runtime_error("Provenance Repo should not be null");
  if (flow_file_repo_ == nullptr)
    throw std::runtime_error("Flow Repo should not be null");
  if (configuration_ == nullptr) {
    throw std::runtime_error("Must supply a configuration.");
  }
  running_ = false;
  initialized_ = false;

  protocol_ = std::make_unique<FlowControlProtocol>(this, configuration_);
  response_node_loader_.setControllerServiceProvider(this);
  response_node_loader_.setStateMonitor(this);
}

FlowController::FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo,
                 std::shared_ptr<Configure> configure, std::unique_ptr<core::FlowConfiguration> flow_configuration,
                 std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<utils::file::FileSystem> filesystem,
                 std::function<void()> request_restart)
      : FlowController(std::move(provenance_repo), std::move(flow_file_repo), std::move(configure), std::move(flow_configuration),
                       std::move(content_repo), DEFAULT_ROOT_GROUP_NAME, std::move(filesystem), std::move(request_restart)) {}

std::optional<std::chrono::milliseconds> FlowController::loadShutdownTimeoutFromConfiguration() {
  std::string shutdown_timeout_str;
  if (configuration_->get(minifi::Configure::nifi_flowcontroller_drain_timeout, shutdown_timeout_str)) {
    const auto time_from_config = core::TimePeriodValue::fromString(shutdown_timeout_str);
    if (time_from_config) {
      return { std::chrono::milliseconds{ time_from_config.value().getMilliseconds() }};
    }
  }
  return std::nullopt;
}

FlowController::~FlowController() {
  stop();
  stopC2();
  unload();
  // TODO(adebreceni): are these here on purpose, so they are destroyed first?
  protocol_ = nullptr;
  flow_file_repo_ = nullptr;
  provenance_repo_ = nullptr;
}

bool FlowController::applyConfiguration(const std::string &source, const std::string &configurePayload) {
  std::unique_ptr<core::ProcessGroup> newRoot;
  try {
    newRoot = flow_configuration_->updateFromPayload(source, configurePayload);
  } catch (const std::exception& ex) {
    logger_->log_error("Invalid configuration payload, type: %s, what: %s", typeid(ex).name(), ex.what());
    return false;
  } catch (...) {
    logger_->log_error("Invalid configuration payload, type: %s", getCurrentExceptionTypeName());
    return false;
  }

  if (newRoot == nullptr)
    return false;

  if (!isRunning())
    return false;

  logger_->log_info("Starting to reload Flow Controller with flow control name %s, version %d", newRoot->getName(), newRoot->getVersion());

  updating_ = true;
  bool started = false;

  {
    std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
    stop();
    unload();
    controller_map_->clear();
    clearResponseNodes();
    if (metrics_publisher_) {
      metrics_publisher_->clearMetricNodes();
    }
    auto prevRoot = std::move(this->root_);
    this->root_ = std::move(newRoot);
    processor_to_controller_.clear();
    initialized_ = false;
    try {
      load(std::move(root_), true);
      flow_update_ = true;
      started = start() == 0;
    } catch (...) {
      this->root_ = std::move(prevRoot);
      load(std::move(this->root_), true);
      flow_update_ = true;
    }
  }

  updating_ = false;

  if (started) {
    auto flowVersion = flow_configuration_->getFlowVersion();
    if (flowVersion) {
      logger_->log_debug("Setting flow id to %s", flowVersion->getFlowId());
      configuration_->set(Configure::nifi_c2_flow_id, flowVersion->getFlowId());
      configuration_->set(Configure::nifi_c2_flow_url, flowVersion->getFlowIdentifier()->getRegistryUrl());
    } else {
      logger_->log_debug("Invalid flow version, not setting");
    }
  }

  return started;
}

int16_t FlowController::stop() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    // immediately indicate that we are not running
    logger_->log_info("Stop Flow Controller");
    if (this->root_) {
      // stop source processors first
      this->root_->stopProcessing(timer_scheduler_, event_scheduler_, cron_scheduler_, [] (const core::Processor* proc) -> bool {
        return !proc->hasIncomingConnections();
      });
      // we enable C2 to progressively increase the timeout
      // in case it sees that waiting for a little longer could
      // allow the FlowFiles to be processed
      auto shutdown_start = std::chrono::steady_clock::now();
      while ((std::chrono::steady_clock::now() - shutdown_start) < loadShutdownTimeoutFromConfiguration().value_or(std::chrono::milliseconds{0}) &&
          this->root_->getTotalFlowFileCount() != 0) {
        std::this_thread::sleep_for(shutdown_check_interval_);
      }
      // shutdown all other processors as well
      this->root_->stopProcessing(timer_scheduler_, event_scheduler_, cron_scheduler_);
    }
    // stop after we've attempted to stop the processors.
    timer_scheduler_->stop();
    event_scheduler_->stop();
    cron_scheduler_->stop();
    thread_pool_.shutdown();
    /* STOP! Before you change it, consider the following:
     * -Stopping the schedulers doesn't actually quit the onTrigger functions of processors
     * -They only guarantee that the processors are not scheduled anymore
     * -After the threadpool is stopped we can make sure that processors don't need repos and controllers anymore */
    if (this->root_) {
      this->root_->drainConnections();
    }
    this->flow_file_repo_->stop();
    this->provenance_repo_->stop();
    // stop the ControllerServices
    this->controller_service_provider_impl_->disableAllControllerServices();
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
void FlowController::waitUnload(const uint64_t timeToWaitMs) {
  if (running_) {
    // use the current time and increment with the provided argument.
    std::chrono::system_clock::time_point wait_time = std::chrono::system_clock::now() + std::chrono::milliseconds(timeToWaitMs);

    // create an asynchronous future.
    std::future<void> unload_task = std::async(std::launch::async, [this]() {unload();});

    if (std::future_status::ready == unload_task.wait_until(wait_time)) {
      running_ = false;
    }
  }
}

void FlowController::unload() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    stop();
  }
  if (initialized_) {
    logger_->log_info("Unload Flow Controller");
    initialized_ = false;
    name_ = "";
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
  controller_service_provider_impl_ = flow_configuration_->getControllerServiceProvider();
  C2Client::initialize(this, this, this);
  auto opt_source = fetchFlow(*opt_flow_url);
  if (!opt_source) {
    logger_->log_error("Couldn't fetch flow configuration from C2 server");
    return nullptr;
  }
  root = flow_configuration_->updateFromPayload(*opt_flow_url, *opt_source);
  if (root) {
    logger_->log_info("Successfully fetched valid flow configuration");
    if (!flow_configuration_->persist(*opt_source)) {
      logger_->log_info("Failed to write the fetched flow to disk");
    }
  }
  return root;
}

void FlowController::load(std::unique_ptr<core::ProcessGroup> root, bool reload) {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    stop();
  }
  if (!initialized_) {
    if (reload) {
      io::NetworkPrioritizerFactory::getInstance()->clearPrioritizer();
    }

    if (root) {
      logger_->log_info("Load Flow Controller from provided root");
      this->root_ = std::move(root);
      processor_to_controller_.clear();
    } else {
      logger_->log_info("Instantiating new flow");
      this->root_ = loadInitialFlow();
    }

    if (root_) {
      root_->verify();
    }

    logger_->log_info("Loaded root processor Group");
    logger_->log_info("Initializing timers");
    controller_service_provider_impl_ = flow_configuration_->getControllerServiceProvider();
    response_node_loader_.initializeComponentMetrics(root_.get());
    initializeResponseNodes(root_.get());
    if (metrics_publisher_) {
      metrics_publisher_->loadMetricNodes(root_.get());
    } else {
      loadMetricsPublisher();
    }

    if (!thread_pool_.isRunning() || reload) {
      thread_pool_.shutdown();
      thread_pool_.setMaxConcurrentTasks(configuration_->getInt(Configure::nifi_flow_engine_threads, 2));
      thread_pool_.setControllerServiceProvider(this);
      thread_pool_.start();
    }

    conditionalReloadScheduler<TimerDrivenSchedulingAgent>(timer_scheduler_, !timer_scheduler_ || reload);
    conditionalReloadScheduler<EventDrivenSchedulingAgent>(event_scheduler_, !event_scheduler_ || reload);
    conditionalReloadScheduler<CronDrivenSchedulingAgent>(cron_scheduler_, !cron_scheduler_ || reload);

    std::static_pointer_cast<core::controller::StandardControllerServiceProvider>(controller_service_provider_impl_)->setRootGroup(root_.get());
    std::static_pointer_cast<core::controller::StandardControllerServiceProvider>(controller_service_provider_impl_)->setSchedulingAgent(
        std::static_pointer_cast<minifi::SchedulingAgent>(event_scheduler_));

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
}

void FlowController::loadFlowRepo() {
  if (this->flow_file_repo_ != nullptr) {
    logger_->log_debug("Getting connection map");
    std::map<std::string, core::Connectable*> connectionMap;
    std::map<std::string, core::Connectable*> containers;
    if (this->root_ != nullptr) {
      this->root_->getConnections(connectionMap);
      this->root_->getFlowFileContainers(containers);
    }
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
  } else {
    if (!running_) {
      logger_->log_info("Starting Flow Controller");
      controller_service_provider_impl_->enableAllControllerServices();
      this->timer_scheduler_->start();
      this->event_scheduler_->start();
      this->cron_scheduler_->start();

      if (this->root_ != nullptr) {
        start_time_ = std::chrono::steady_clock::now();
        // watch out, this might immediately start the processors
        // as the thread_pool_ is started in load()
        this->root_->startProcessing(timer_scheduler_, event_scheduler_, cron_scheduler_);
      }
      C2Client::initialize(this, this, this);
      core::logging::LoggerConfiguration::getConfiguration().initializeAlertSinks(this, configuration_);
      running_ = true;
      this->protocol_->start();
      this->provenance_repo_->start();
      this->flow_file_repo_->start();
      thread_pool_.start();
      logger_->log_info("Started Flow Controller");
    }
    return 0;
  }
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

int16_t FlowController::applyUpdate(const std::string &source, const std::string &configuration, bool persist) {
  if (applyConfiguration(source, configuration)) {
    if (persist) {
      flow_configuration_->persist(configuration);
    }
    return 0;
  } else {
    return -1;
  }
}

int16_t FlowController::clearConnection(const std::string &connection) {
  if (root_ != nullptr) {
    logger_->log_info("Attempting to clear connection %s", connection);
    std::map<std::string, Connection*> connections;
    root_->getConnections(connections);
    auto conn = connections.find(connection);
    if (conn != connections.end()) {
      logger_->log_info("Clearing connection %s", connection);
      conn->second->drain(true);
    }
  }
  return -1;
}

state::response::NodeReporter::ReportedNode FlowController::getAgentManifest() {
  state::response::AgentInformation agentInfo("agentInfo");
  agentInfo.setUpdatePolicyController(std::static_pointer_cast<controllers::UpdatePolicyControllerService>(getControllerService(c2::C2Agent::UPDATE_NAME)).get());
  agentInfo.setAgentIdentificationProvider(configuration_);
  agentInfo.setConfigurationReader([this](const std::string& key){
    return configuration_->getRawValue(key);
  });
  agentInfo.setStateMonitor(this);
  agentInfo.includeAgentStatus(false);
  state::response::NodeReporter::ReportedNode reported_node;
  reported_node.name = agentInfo.getName();
  reported_node.is_array = agentInfo.isArray();
  reported_node.serialized_nodes = agentInfo.serialize();
  return reported_node;
}

void FlowController::executeOnAllComponents(std::function<void(state::StateController&)> func) {
  if (updating_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  for (auto* component: getAllComponents()) {
    func(*component);
  }
}

void FlowController::executeOnComponent(const std::string &id_or_name, std::function<void(state::StateController&)> func) {
  if (updating_) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (auto* component = getComponent(id_or_name); component != nullptr) {
    func(*component);
  } else {
    logger_->log_error("Could not get execute requested callback for component \"%s\", because component was not found", id_or_name);
  }
}

std::vector<state::StateController*> FlowController::getAllComponents() {
  if (root_) {
    return getAllProcessorControllers([this](core::Processor& p) { return createController(p); });
  }

  return {this};
}

state::StateController* FlowController::getComponent(const std::string& id_or_name) {
  if (id_or_name == getUUIDStr() || id_or_name == "FlowController") {
    return this;
  } else if (root_) {
    return getProcessorController(id_or_name, [this](core::Processor& p) { return createController(p); });
  }

  return nullptr;
}

gsl::not_null<std::unique_ptr<state::ProcessorController>> FlowController::createController(core::Processor& processor) {
  const auto scheduler = [this, &processor]() -> std::shared_ptr<SchedulingAgent> {
    switch (processor.getSchedulingStrategy()) {
      case core::SchedulingStrategy::TIMER_DRIVEN: return timer_scheduler_;
      case core::SchedulingStrategy::EVENT_DRIVEN: return event_scheduler_;
      case core::SchedulingStrategy::CRON_DRIVEN: return cron_scheduler_;
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
  if (auto logs = core::logging::LoggerConfiguration::getCompressedLog(true)) {
    debug_info["minifi.log.gz"] = std::move(logs);
  }
  if (auto opt_flow_path = flow_configuration_->getConfigurationPath()) {
    debug_info["config.yml"] = std::make_unique<io::FileStream>(opt_flow_path.value(), 0, false);
  }
  debug_info["minifi.properties"] = std::make_unique<io::FileStream>(configuration_->getFilePath(), 0, false);

  return debug_info;
}

std::vector<state::StateController*> FlowController::getAllProcessorControllers(
        const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory) {
  std::vector<state::StateController*> controllerVec{this};
  std::vector<core::Processor*> processorVec;
  root_->getAllProcessors(processorVec);

  for (const auto& processor : processorVec) {
    // reference to the existing or newly created controller
    auto& controller = processor_to_controller_[processor->getUUID()];
    if (!controller) {
      controller = controllerFactory(*processor);
    }
    controllerVec.push_back(controller.get());
  }

  return controllerVec;
}

state::StateController* FlowController::getProcessorController(const std::string& id_or_name,
    const std::function<gsl::not_null<std::unique_ptr<state::ProcessorController>>(core::Processor&)>& controllerFactory) {
  return utils::Identifier::parse(id_or_name)
      | utils::flatMap([this](utils::Identifier id) { return utils::optional_from_ptr(root_->findProcessorById(id)); })
      | utils::orElse([this, &id_or_name] { return utils::optional_from_ptr(root_->findProcessorByName(id_or_name)); })
      | utils::map([this, &controllerFactory](gsl::not_null<core::Processor*> proc) -> gsl::not_null<state::ProcessorController*> {
        return utils::optional_from_ptr(processor_to_controller_[proc->getUUID()].get())
            | utils::valueOrElse([this, proc, &controllerFactory] {
              return gsl::make_not_null((processor_to_controller_[proc->getUUID()] = controllerFactory(*proc)).get());
            });
      })
      | utils::valueOrElse([this, &id_or_name]() -> state::ProcessorController* {
        logger_->log_error("Could not get processor controller for requested id/name \"%s\", because the processor was not found", id_or_name);
        return nullptr;
      });
}

void FlowController::loadMetricsPublisher() {
  if (auto metrics_publisher_class = configuration_->get(minifi::Configure::nifi_metrics_publisher_class)) {
    auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(*metrics_publisher_class, *metrics_publisher_class);
    if (!ptr) {
      logger_->log_error("Configured metrics publisher class \"%s\" could not be instantiated.", *metrics_publisher_class);
      return;
    }
    metrics_publisher_ = utils::dynamic_unique_cast<state::MetricsPublisher>(std::move(ptr));
    if (!metrics_publisher_) {
      logger_->log_error("Configured metrics publisher class \"%s\" is not a metrics publisher.", *metrics_publisher_class);
      return;
    }
    metrics_publisher_->initialize(configuration_, response_node_loader_, root_.get());
  }
}

}  // namespace org::apache::nifi::minifi
