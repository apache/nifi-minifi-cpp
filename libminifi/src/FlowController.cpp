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
#include "FlowController.h"
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <chrono>
#include <future>
#include <thread>
#include <utility>
#include <memory>
#include <string>
#include "core/state/metrics/QueueMetrics.h"
#include "core/state/metrics/DeviceInformation.h"
#include "core/state/metrics/SystemMetrics.h"
#include "core/state/metrics/ProcessMetrics.h"
#include "core/state/metrics/RepositoryMetrics.h"
#include "core/state/ProcessorController.h"
#include "yaml-cpp/yaml.h"
#include "c2/C2Agent.h"
#include "core/ProcessContext.h"
#include "core/ProcessGroup.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/ClassLoader.h"
#include "SchedulingAgent.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Connectable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::shared_ptr<utils::IdGenerator> FlowController::id_generator_ = utils::IdGenerator::getIdGenerator();

#define DEFAULT_CONFIG_NAME "conf/flow.yml"

FlowController::FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                               std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<core::ContentRepository> content_repo, const std::string name, bool headless_mode)
    : core::controller::ControllerServiceProvider(core::getClassName<FlowController>()),
      root_(nullptr),
      max_timer_driven_threads_(0),
      max_event_driven_threads_(0),
      running_(false),
      c2_enabled_(true),
      initialized_(false),
      provenance_repo_(provenance_repo),
      flow_file_repo_(flow_file_repo),
      protocol_(0),
      controller_service_map_(std::make_shared<core::controller::ControllerServiceMap>()),
      timer_scheduler_(nullptr),
      event_scheduler_(nullptr),
      controller_service_provider_(nullptr),
      flow_configuration_(std::move(flow_configuration)),
      configuration_(configure),
      content_repo_(content_repo),
      logger_(logging::LoggerFactory<FlowController>::getLogger()) {
  if (provenance_repo == nullptr)
    throw std::runtime_error("Provenance Repo should not be null");
  if (flow_file_repo == nullptr)
    throw std::runtime_error("Flow Repo should not be null");
  if (IsNullOrEmpty(configuration_)) {
    throw std::runtime_error("Must supply a configuration.");
  }
  id_generator_->generate(uuid_);
  setUUID(uuid_);

  // Setup the default values
  if (flow_configuration_ != nullptr) {
    configuration_filename_ = flow_configuration_->getConfigurationPath();
  }
  max_event_driven_threads_ = DEFAULT_MAX_EVENT_DRIVEN_THREAD;
  max_timer_driven_threads_ = DEFAULT_MAX_TIMER_DRIVEN_THREAD;
  running_ = false;
  initialized_ = false;
  c2_initialized_ = false;
  root_ = nullptr;

  protocol_ = new FlowControlProtocol(this, configure);

  if (!headless_mode) {
    std::string rawConfigFileString;
    configure->get(Configure::nifi_flow_configuration_file, rawConfigFileString);

    if (!rawConfigFileString.empty()) {
      configuration_filename_ = rawConfigFileString;
    }

    std::string adjustedFilename;
    if (!configuration_filename_.empty()) {
      // perform a naive determination if this is a relative path
      if (configuration_filename_.c_str()[0] != '/') {
        adjustedFilename = adjustedFilename + configure->getHome() + "/" + configuration_filename_;
      } else {
        adjustedFilename = configuration_filename_;
      }
    }

    initializePaths(adjustedFilename);
  }
}

void FlowController::initializePaths(const std::string &adjustedFilename) {
  char *path = NULL;
  char full_path[PATH_MAX];
  path = realpath(adjustedFilename.c_str(), full_path);

  if (path == NULL) {
    throw std::runtime_error("Path is not specified. Either manually set MINIFI_HOME or ensure ../conf exists");
  }
  std::string pathString(path);
  configuration_filename_ = pathString;
  logger_->log_info("FlowController NiFi Configuration file %s", pathString.c_str());

  // Create the content repo directory if needed
  struct stat contentDirStat;

  minifi::setDefaultDirectory(DEFAULT_CONTENT_DIRECTORY);

  if (stat(minifi::default_directory_path.c_str(), &contentDirStat) != -1 && S_ISDIR(contentDirStat.st_mode)) {
    path = realpath(minifi::default_directory_path.c_str(), full_path);
    logger_->log_info("FlowController content directory %s", full_path);
  } else {
    if (mkdir(minifi::default_directory_path.c_str(), 0777) == -1) {
      logger_->log_error("FlowController content directory creation failed");
      exit(1);
    }
  }

  std::string clientAuthStr;

  if (!path) {
    logger_->log_error("Could not locate path from provided configuration file name (%s).  Exiting.", full_path);
    exit(1);
  }
}

FlowController::~FlowController() {
  stop(true);
  unload();
  if (NULL != protocol_)
    delete protocol_;
  flow_file_repo_ = nullptr;
  provenance_repo_ = nullptr;
}

bool FlowController::applyConfiguration(const std::string &configurePayload) {
  std::unique_ptr<core::ProcessGroup> newRoot;
  try {
    newRoot = std::move(flow_configuration_->getRootFromPayload(configurePayload));
  } catch (...) {
    logger_->log_error("Invalid configuration payload");
    return false;
  }

  if (newRoot == nullptr)
    return false;

  logger_->log_info("Starting to reload Flow Controller with flow control name %s, version %d", newRoot->getName().c_str(), newRoot->getVersion());

  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  stop(true);
  waitUnload(30000);
  this->root_ = std::move(newRoot);
  loadFlowRepo();
  initialized_ = true;
  return start();
}

int16_t FlowController::stop(bool force, uint64_t timeToWait) {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    // immediately indicate that we are not running
    logger_->log_info("Stop Flow Controller");
    if (this->root_)
      this->root_->stopProcessing(this->timer_scheduler_.get(), this->event_scheduler_.get());
    this->flow_file_repo_->stop();
    this->provenance_repo_->stop();
    // stop after we've attempted to stop the processors.
    this->timer_scheduler_->stop();
    this->event_scheduler_->stop();
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
    stop(true);
  }
  if (initialized_) {
    logger_->log_info("Unload Flow Controller");
    initialized_ = false;
    name_ = "";
  }

  return;
}

void FlowController::load() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    stop(true);
  }
  if (!initialized_) {
    logger_->log_info("Initializing timers");

    if (nullptr == timer_scheduler_) {
      timer_scheduler_ = std::make_shared<TimerDrivenSchedulingAgent>(
          std::static_pointer_cast<core::controller::ControllerServiceProvider>(std::dynamic_pointer_cast<FlowController>(shared_from_this())), provenance_repo_, flow_file_repo_, content_repo_,
          configuration_);
    }
    if (nullptr == event_scheduler_) {
      event_scheduler_ = std::make_shared<EventDrivenSchedulingAgent>(
          std::static_pointer_cast<core::controller::ControllerServiceProvider>(std::dynamic_pointer_cast<FlowController>(shared_from_this())), provenance_repo_, flow_file_repo_, content_repo_,
          configuration_);
    }
    logger_->log_info("Load Flow Controller from file %s", configuration_filename_.c_str());

    this->root_ = std::shared_ptr<core::ProcessGroup>(flow_configuration_->getRoot(configuration_filename_));

    logger_->log_info("Loaded root processor Group");

    controller_service_provider_ = flow_configuration_->getControllerServiceProvider();

    std::static_pointer_cast<core::controller::StandardControllerServiceProvider>(controller_service_provider_)->setRootGroup(root_);
    std::static_pointer_cast<core::controller::StandardControllerServiceProvider>(controller_service_provider_)->setSchedulingAgent(
        std::static_pointer_cast<minifi::SchedulingAgent>(event_scheduler_));

    logger_->log_info("Loaded controller service provider");
    // Load Flow File from Repo
    loadFlowRepo();
    logger_->log_info("Loaded flow repository");
    initialized_ = true;
  }
}

void FlowController::reload(std::string yamlFile) {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  logger_->log_info("Starting to reload Flow Controller with yaml %s", yamlFile.c_str());
  stop(true);
  unload();
  std::string oldYamlFile = this->configuration_filename_;
  this->configuration_filename_ = yamlFile;
  load();
  start();
  if (this->root_ != nullptr) {
    this->configuration_filename_ = oldYamlFile;
    logger_->log_info("Rollback Flow Controller to YAML %s", oldYamlFile.c_str());
    stop(true);
    unload();
    load();
    start();
  }
}

void FlowController::loadFlowRepo() {
  if (this->flow_file_repo_ != nullptr) {
    logger_->log_debug("Getting connection map");
    std::map<std::string, std::shared_ptr<core::Connectable>> connectionMap;
    if (this->root_ != nullptr) {
      this->root_->getConnections(connectionMap);
    }
    logger_->log_debug("Number of connections from connectionMap %d", connectionMap.size());
    flow_file_repo_->setConnectionMap(connectionMap);
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
      controller_service_provider_->enableAllControllerServices();
      this->timer_scheduler_->start();
      this->event_scheduler_->start();

      if (this->root_ != nullptr) {
        start_time_ = std::chrono::steady_clock::now();
        this->root_->startProcessing(this->timer_scheduler_.get(), this->event_scheduler_.get());
      }
      initializeC2();
      running_ = true;
      this->protocol_->start();
      this->provenance_repo_->start();
      this->flow_file_repo_->start();
      logger_->log_info("Started Flow Controller");
    }
    return 0;
  }
}

void FlowController::initializeC2() {
  if (!c2_enabled_) {
    return;
  }
  if (!c2_initialized_) {
    std::string c2_enable_str;

    if (configuration_->get(Configure::nifi_c2_enable, c2_enable_str)) {
      bool enable_c2 = true;
      utils::StringUtils::StringToBool(c2_enable_str, enable_c2);
      c2_enabled_ = enable_c2;
      if (!c2_enabled_) {
        return;
      }
    } else {
      c2_enabled_ = true;
    }
    state::StateManager::initialize();
    std::shared_ptr<c2::C2Agent> agent = std::make_shared<c2::C2Agent>(std::dynamic_pointer_cast<FlowController>(shared_from_this()), std::dynamic_pointer_cast<FlowController>(shared_from_this()),
                                                                       configuration_);
    registerUpdateListener(agent);
  }
  if (!c2_enabled_) {
    return;
  }

  c2_initialized_ = true;
  metrics_.clear();
  component_metrics_.clear();
  component_metrics_by_id_.clear();
  std::string class_csv;

  if (root_ != nullptr) {
    std::shared_ptr<state::metrics::QueueMetrics> queueMetrics = std::make_shared<state::metrics::QueueMetrics>();

    std::map<std::string, std::shared_ptr<Connection>> connections;
    root_->getConnections(connections);
    for (auto con : connections) {
      queueMetrics->addConnection(con.second);
    }
    metrics_[queueMetrics->getName()] = queueMetrics;

    std::shared_ptr<state::metrics::RepositoryMetrics> repoMetrics = std::make_shared<state::metrics::RepositoryMetrics>();

    repoMetrics->addRepository(provenance_repo_);
    repoMetrics->addRepository(flow_file_repo_);

    metrics_[repoMetrics->getName()] = repoMetrics;
  }

  if (configuration_->get("nifi.flow.metrics.classes", class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (std::string clazz : classes) {
      auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);

      if (nullptr == ptr) {
        logger_->log_error("No metric defined for %s", clazz.c_str());
        continue;
      }

      std::shared_ptr<state::metrics::Metrics> processor = std::static_pointer_cast<state::metrics::Metrics>(ptr);

      std::lock_guard<std::mutex> lock(metrics_mutex_);

      metrics_[processor->getName()] = processor;
    }
  }

  // first we should get all component metrics, then
  // we will build the mapping
  std::vector<std::shared_ptr<core::Processor>> processors;
  if (root_ != nullptr) {
    root_->getAllProcessors(processors);
    for (const auto &processor : processors) {
      auto rep = std::dynamic_pointer_cast<state::metrics::MetricsSource>(processor);
      // we have a metrics source.
      if (nullptr != rep) {
        std::vector<std::shared_ptr<state::metrics::Metrics>> metric_vector;
        rep->getMetrics(metric_vector);
        for (auto metric : metric_vector) {
          component_metrics_[metric->getName()] = metric;
        }
      }
    }
  }

  std::string class_definitions;
  if (configuration_->get("nifi.flow.metrics.class.definitions", class_definitions)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

    for (std::string metricsClass : classes) {
      try {
        int id = std::stoi(metricsClass);
        std::stringstream option;
        option << "nifi.flow.metrics.class.definitions." << metricsClass;
        if (configuration_->get(option.str(), class_definitions)) {
          std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

          for (std::string clazz : classes) {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            auto ret = component_metrics_[clazz];
            if (nullptr == ret) {
              ret = metrics_[clazz];
            }
            if (nullptr == ret) {
              logger_->log_error("No metric defined for %s", clazz.c_str());
              continue;
            }
            component_metrics_by_id_[id].push_back(ret);
          }
        }
      } catch (...) {
        logger_->log_error("Could not create metrics class %s", metricsClass);
      }
    }
  }
}
/**
 * Controller Service functions
 *
 */

/**
 * Creates a controller service through the controller service provider impl.
 * @param type class name
 * @param id service identifier
 * @param firstTimeAdded first time this CS was added
 */
std::shared_ptr<core::controller::ControllerServiceNode> FlowController::createControllerService(const std::string &type, const std::string &id, bool firstTimeAdded) {
  return controller_service_provider_->createControllerService(type, id, firstTimeAdded);
}

/**
 * controller service provider
 */
/**
 * removes controller service
 * @param serviceNode service node to be removed.
 */

void FlowController::removeControllerService(const std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  controller_map_->removeControllerService(serviceNode);
}

/**
 * Enables the controller service services
 * @param serviceNode service node which will be disabled, along with linked services.
 */
std::future<bool> FlowController::enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->enableControllerService(serviceNode);
}

/**
 * Enables controller services
 * @param serviceNoden vector of service nodes which will be enabled, along with linked services.
 */
void FlowController::enableControllerServices(std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> serviceNodes) {
}

/**
 * Disables controller services
 * @param serviceNode service node which will be disabled, along with linked services.
 */
std::future<bool> FlowController::disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->disableControllerService(serviceNode);
}

/**
 * Gets all controller services.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::getAllControllerServices() {
  return controller_service_provider_->getAllControllerServices();
}

/**
 * Gets controller service node specified by <code>id</code>
 * @param id service identifier
 * @return shared pointer to the controller service node or nullptr if it does not exist.
 */
std::shared_ptr<core::controller::ControllerServiceNode> FlowController::getControllerServiceNode(const std::string &id) {
  return controller_service_provider_->getControllerServiceNode(id);
}

void FlowController::verifyCanStopReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
}

/**
 * Unschedules referencing components.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::unscheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->unscheduleReferencingComponents(serviceNode);
}

/**
 * Verify can disable referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
void FlowController::verifyCanDisableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  controller_service_provider_->verifyCanDisableReferencingServices(serviceNode);
}

/**
 * Disables referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::disableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->disableReferencingServices(serviceNode);
}

/**
 * Verify can enable referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
void FlowController::verifyCanEnableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  controller_service_provider_->verifyCanEnableReferencingServices(serviceNode);
}

/**
 * Determines if the controller service specified by identifier is enabled.
 */
bool FlowController::isControllerServiceEnabled(const std::string &identifier) {
  return controller_service_provider_->isControllerServiceEnabled(identifier);
}

/**
 * Enables referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::enableReferencingServices(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->enableReferencingServices(serviceNode);
}

/**
 * Schedules referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::scheduleReferencingComponents(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->scheduleReferencingComponents(serviceNode);
}

/**
 * Returns controller service components referenced by serviceIdentifier from the embedded
 * controller service provider;
 */
std::shared_ptr<core::controller::ControllerService> FlowController::getControllerServiceForComponent(const std::string &serviceIdentifier, const std::string &componentId) {
  return controller_service_provider_->getControllerServiceForComponent(serviceIdentifier, componentId);
}

/**
 * Enables all controller services for the provider.
 */
void FlowController::enableAllControllerServices() {
  controller_service_provider_->enableAllControllerServices();
}

int16_t FlowController::applyUpdate(const std::string &configuration) {
  applyConfiguration(configuration);
  return 0;
}

int16_t FlowController::clearConnection(const std::string &connection) {
  if (root_ != nullptr) {
    logger_->log_info("Attempting to clear connection %s", connection);
    std::map<std::string, std::shared_ptr<Connection>> connections;
    root_->getConnections(connections);
    auto conn = connections.find(connection);
    if (conn != connections.end()) {
      logger_->log_info("Clearing connection %s", connection);
      conn->second->drain();
    }
  }
  return -1;
}

int16_t FlowController::getMetrics(std::vector<std::shared_ptr<state::metrics::Metrics>> &metric_vector, uint8_t metricsClass) {
  auto now = std::chrono::steady_clock::now();
  auto time_since = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_metrics_capture_).count();
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (metricsClass == 0) {
    for (auto metric : metrics_) {
      metric_vector.push_back(metric.second);
    }
  } else {
    auto metrics = component_metrics_by_id_[metricsClass];
    for (const auto &metric : metrics) {
      metric_vector.push_back(metric);
    }
  }
  return 0;
}

std::vector<std::shared_ptr<state::StateController>> FlowController::getAllComponents() {
  std::vector<std::shared_ptr<state::StateController>> vec;
  vec.push_back(shared_from_this());
  std::vector<std::shared_ptr<core::Processor>> processors;
  if (root_ != nullptr) {
    root_->getAllProcessors(processors);
    for (auto &processor : processors) {
      switch (processor->getSchedulingStrategy()) {
        case core::SchedulingStrategy::TIMER_DRIVEN:
          vec.push_back(std::make_shared<state::ProcessorController>(processor, timer_scheduler_));
          break;
        case core::SchedulingStrategy::EVENT_DRIVEN:
          vec.push_back(std::make_shared<state::ProcessorController>(processor, event_scheduler_));
          break;
        default:
          break;
      }
    }
  }
  return vec;
}
std::vector<std::shared_ptr<state::StateController>> FlowController::getComponents(const std::string &name) {
  std::vector<std::shared_ptr<state::StateController>> vec;

  if (name == "FlowController") {
    vec.push_back(shared_from_this());
  } else {
    // check processors
    std::shared_ptr<core::Processor> processor = root_->findProcessor(name);
    if (processor != nullptr) {
      switch (processor->getSchedulingStrategy()) {
        case core::SchedulingStrategy::TIMER_DRIVEN:
          vec.push_back(std::make_shared<state::ProcessorController>(processor, timer_scheduler_));
          break;
        case core::SchedulingStrategy::EVENT_DRIVEN:
          vec.push_back(std::make_shared<state::ProcessorController>(processor, event_scheduler_));
          break;
        default:
          break;
      }
    }
  }

  return vec;
}

uint64_t FlowController::getUptime() {
  auto now = std::chrono::steady_clock::now();
  auto time_since = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
  return time_since;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
