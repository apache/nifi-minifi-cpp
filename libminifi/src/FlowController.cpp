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
#include "core/ProcessContext.h"
#include "core/ProcessGroup.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/controller/ControllerServiceProvider.h"
#include "core/repository/FlowFileRepository.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

#define DEFAULT_CONFIG_NAME "conf/flow.yml"

FlowController::FlowController(
    std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo,
    std::shared_ptr<Configure> configure,
    std::unique_ptr<core::FlowConfiguration> flow_configuration,
    const std::string name, bool headless_mode)
    : core::controller::ControllerServiceProvider(
          core::getClassName<FlowController>()),
      root_(nullptr),
      max_timer_driven_threads_(0),
      max_event_driven_threads_(0),
      running_(false),
      initialized_(false),
      provenance_repo_(provenance_repo),
      flow_file_repo_(flow_file_repo),
      protocol_(0),
      controller_service_map_(
          std::make_shared<core::controller::ControllerServiceMap>()),
      timer_scheduler_(nullptr),
      event_scheduler_(nullptr),
      controller_service_provider_(nullptr),
      flow_configuration_(std::move(flow_configuration)),
      configuration_(configure),
      logger_(logging::LoggerFactory<FlowController>::getLogger())  {
  if (provenance_repo == nullptr)
    throw std::runtime_error("Provenance Repo should not be null");
  if (flow_file_repo == nullptr)
    throw std::runtime_error("Flow Repo should not be null");
  if (IsNullOrEmpty(configuration_)) {
    throw std::runtime_error("Must supply a configuration.");
  }
  uuid_generate(uuid_);
  setUUID(uuid_);

  // Setup the default values
  if (flow_configuration_ != nullptr) {
    configuration_filename_ = flow_configuration_->getConfigurationPath();
  }
  max_event_driven_threads_ = DEFAULT_MAX_EVENT_DRIVEN_THREAD;
  max_timer_driven_threads_ = DEFAULT_MAX_TIMER_DRIVEN_THREAD;
  running_ = false;
  initialized_ = false;
  root_ = nullptr;

  protocol_ = new FlowControlProtocol(this, configure);

  if (!headless_mode) {
    std::string rawConfigFileString;
    configure->get(Configure::nifi_flow_configuration_file,
                   rawConfigFileString);

    if (!rawConfigFileString.empty()) {
      configuration_filename_ = rawConfigFileString;
    }

    std::string adjustedFilename;
    if (!configuration_filename_.empty()) {
      // perform a naive determination if this is a relative path
      if (configuration_filename_.c_str()[0] != '/') {
        adjustedFilename = adjustedFilename + configure->getHome() + "/"
            + configuration_filename_;
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
    throw std::runtime_error(
        "Path is not specified. Either manually set MINIFI_HOME or ensure ../conf exists");
  }
  std::string pathString(path);
  configuration_filename_ = pathString;
  logger_->log_info("FlowController NiFi Configuration file %s",
                    pathString.c_str());

  // Create the content repo directory if needed
  struct stat contentDirStat;

  if (stat(ResourceClaim::default_directory_path, &contentDirStat)
      != -1&& S_ISDIR(contentDirStat.st_mode)) {
    path = realpath(ResourceClaim::default_directory_path, full_path);
    logger_->log_info("FlowController content directory %s", full_path);
  } else {
    if (mkdir(ResourceClaim::default_directory_path, 0777) == -1) {
      logger_->log_error("FlowController content directory creation failed");
      exit(1);
    }
  }

  std::string clientAuthStr;

  if (!path) {
    logger_->log_error(
        "Could not locate path from provided configuration file name (%s).  Exiting.",
        full_path);
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

void FlowController::stop(bool force) {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    // immediately indicate that we are not running
    running_ = false;

    logger_->log_info("Stop Flow Controller");
    this->timer_scheduler_->stop();
    this->event_scheduler_->stop();
    this->flow_file_repo_->stop();
    this->provenance_repo_->stop();
    // Wait for sometime for thread stop
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (this->root_)
      this->root_->stopProcessing(this->timer_scheduler_.get(),
                                  this->event_scheduler_.get());
  }
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
    std::chrono::system_clock::time_point wait_time =
        std::chrono::system_clock::now()
            + std::chrono::milliseconds(timeToWaitMs);

    // create an asynchronous future.
    std::future<void> unload_task = std::async(std::launch::async,
                                               [this]() {unload();});

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
    root_ = nullptr;
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
          std::static_pointer_cast<core::controller::ControllerServiceProvider>(
              shared_from_this()),
          provenance_repo_, configuration_);
    }
    if (nullptr == event_scheduler_) {
      event_scheduler_ = std::make_shared<EventDrivenSchedulingAgent>(
          std::static_pointer_cast<core::controller::ControllerServiceProvider>(
              shared_from_this()),
          provenance_repo_, configuration_);
    }
    logger_->log_info("Load Flow Controller from file %s",
                      configuration_filename_.c_str());

    this->root_ = std::shared_ptr<core::ProcessGroup>(
        flow_configuration_->getRoot(configuration_filename_));

    logger_->log_info("Loaded root processor Group");

    controller_service_provider_ = flow_configuration_
        ->getControllerServiceProvider();

    std::static_pointer_cast<core::controller::StandardControllerServiceProvider>(
        controller_service_provider_)->setRootGroup(root_);
    std::static_pointer_cast<core::controller::StandardControllerServiceProvider>(
        controller_service_provider_)->setSchedulingAgent(
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
  logger_->log_info("Starting to reload Flow Controller with yaml %s",
                    yamlFile.c_str());
  stop(true);
  unload();
  std::string oldYamlFile = this->configuration_filename_;
  this->configuration_filename_ = yamlFile;
  load();
  start();
  if (this->root_ != nullptr) {
    this->configuration_filename_ = oldYamlFile;
    logger_->log_info("Rollback Flow Controller to YAML %s",
                      oldYamlFile.c_str());
    stop(true);
    unload();
    load();
    start();
  }
}

void FlowController::loadFlowRepo() {
  if (this->flow_file_repo_ != nullptr) {
    logger_->log_debug("Getting connection map");
    std::map<std::string, std::shared_ptr<Connection>> connectionMap;
    if (this->root_ != nullptr) {
      this->root_->getConnections(connectionMap);
    }
    logger_->log_debug("Number of connections from connectionMap %d",
                       connectionMap.size());
    auto rep = std::dynamic_pointer_cast<core::repository::FlowFileRepository>(
        flow_file_repo_);
    if (nullptr != rep) {
      rep->setConnectionMap(connectionMap);
    }
    flow_file_repo_->loadComponent();
  } else {
    logger_->log_debug("Flow file repository is not set");
  }
}

bool FlowController::start() {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (!initialized_) {
    logger_->log_error(
        "Can not start Flow Controller because it has not been initialized");
    return false;
  } else {
    if (!running_) {
      logger_->log_info("Starting Flow Controller");
      controller_service_provider_->enableAllControllerServices();
      this->timer_scheduler_->start();
      this->event_scheduler_->start();
      if (this->root_ != nullptr) {
        this->root_->startProcessing(this->timer_scheduler_.get(),
                                     this->event_scheduler_.get());
      }
      running_ = true;
      this->protocol_->start();
      this->provenance_repo_->start();
      this->flow_file_repo_->start();
      logger_->log_info("Started Flow Controller");
    }
    return true;
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
std::shared_ptr<core::controller::ControllerServiceNode> FlowController::createControllerService(
    const std::string &type, const std::string &id,
    bool firstTimeAdded) {
  return controller_service_provider_->createControllerService(type, id,
                                                               firstTimeAdded);
}

/**
 * controller service provider
 */
/**
 * removes controller service
 * @param serviceNode service node to be removed.
 */

void FlowController::removeControllerService(
    const std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  controller_map_->removeControllerService(serviceNode);
}

/**
 * Enables the controller service services
 * @param serviceNode service node which will be disabled, along with linked services.
 */
void FlowController::enableControllerService(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->enableControllerService(serviceNode);
}

/**
 * Enables controller services
 * @param serviceNoden vector of service nodes which will be enabled, along with linked services.
 */
void FlowController::enableControllerServices(
    std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> serviceNodes) {
}

/**
 * Disables controller services
 * @param serviceNode service node which will be disabled, along with linked services.
 */
void FlowController::disableControllerService(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  controller_service_provider_->disableControllerService(serviceNode);
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
std::shared_ptr<core::controller::ControllerServiceNode> FlowController::getControllerServiceNode(
    const std::string &id) {
  return controller_service_provider_->getControllerServiceNode(id);
}

void FlowController::verifyCanStopReferencingComponents(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
}

/**
 * Unschedules referencing components.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::unscheduleReferencingComponents(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->unscheduleReferencingComponents(
      serviceNode);
}

/**
 * Verify can disable referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
void FlowController::verifyCanDisableReferencingServices(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  controller_service_provider_->verifyCanDisableReferencingServices(
      serviceNode);
}

/**
 * Disables referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::disableReferencingServices(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->disableReferencingServices(serviceNode);
}

/**
 * Verify can enable referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
void FlowController::verifyCanEnableReferencingServices(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
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
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::enableReferencingServices(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->enableReferencingServices(serviceNode);
}

/**
 * Schedules referencing components
 * @param serviceNode service node whose referenced components will be scheduled.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::scheduleReferencingComponents(
    std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->scheduleReferencingComponents(
      serviceNode);
}

/**
 * Returns controller service components referenced by serviceIdentifier from the embedded
 * controller service provider;
 */
std::shared_ptr<core::controller::ControllerService> FlowController::getControllerServiceForComponent(
    const std::string &serviceIdentifier, const std::string &componentId) {
  return controller_service_provider_->getControllerServiceForComponent(
      serviceIdentifier, componentId);
}

/**
 * Enables all controller services for the provider.
 */
void FlowController::enableAllControllerServices() {
  controller_service_provider_->enableAllControllerServices();
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
