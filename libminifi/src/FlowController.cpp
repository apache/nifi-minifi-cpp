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
#include "core/repository/FlowFileRepository.h"

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
    : CoreComponent(core::getClassName<FlowController>()),
      root_(nullptr),
      max_timer_driven_threads_(0),
      max_event_driven_threads_(0),
      running_(false),
      initialized_(false),
      provenance_repo_(provenance_repo),
      flow_file_repo_(flow_file_repo),
      protocol_(0),
      _timerScheduler(provenance_repo_, configure),
      _eventScheduler(provenance_repo_, configure),
      flow_configuration_(std::move(flow_configuration)) {
  if (provenance_repo == nullptr)
    throw std::runtime_error("Provenance Repo should not be null");
  if (flow_file_repo == nullptr)
    throw std::runtime_error("Flow Repo should not be null");

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
  root_ = NULL;

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
    this->_timerScheduler.stop();
    this->_eventScheduler.stop();
    this->flow_file_repo_->stop();
    this->provenance_repo_->stop();
    // Wait for sometime for thread stop
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (this->root_)
      this->root_->stopProcessing(&this->_timerScheduler,
                                  &this->_eventScheduler);
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
    logger_->log_info("Load Flow Controller from file %s",
                      configuration_filename_.c_str());

    this->root_ = flow_configuration_->getRoot(configuration_filename_);

    // Load Flow File from Repo
    loadFlowRepo();

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
  if (this->flow_file_repo_) {
    std::map<std::string, std::shared_ptr<Connection>> connectionMap;
    if (this->root_ != nullptr) {
      this->root_->getConnections(connectionMap);
    }
    auto rep = std::static_pointer_cast<core::repository::FlowFileRepository>(
        flow_file_repo_);
    rep->setConnectionMap(connectionMap);
    flow_file_repo_->loadComponent();
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
      this->_timerScheduler.start();
      this->_eventScheduler.start();
      if (this->root_ != nullptr) {
        this->root_->startProcessing(&this->_timerScheduler,
                                     &this->_eventScheduler);
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

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
