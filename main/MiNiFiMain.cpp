/**
 * @file MiNiFiMain.cpp 
 * MiNiFiMain implementation 
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
#include <fcntl.h>
#include <stdio.h>
#include <semaphore.h>
#include <signal.h>
#include <vector>
#include <queue>
#include <map>
#include <unistd.h>
#include <yaml-cpp/yaml.h>
#include <iostream>

#include "core/logging/BaseLogger.h"
#include "core/logging/LogAppenders.h"
#include "spdlog/spdlog.h"

#include "core/logging/Logger.h"
#include "Configure.h"
#include "FlowController.h"

//! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME_MS 30*1000
//! Default YAML location
#define DEFAULT_NIFI_CONFIG_YML "./conf/config.yml"
//! Default nifi properties file path
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/minifi.properties"
//! Define home environment variable
#define MINIFI_HOME_ENV_KEY "MINIFI_HOME"

/* Define Parser Values for Configuration YAML sections */
#define CONFIG_YAML_PROCESSORS_KEY "Processors"
#define CONFIG_YAML_FLOW_CONTROLLER_KEY "Flow Controller"
#define CONFIG_YAML_CONNECTIONS_KEY "Connections"
#define CONFIG_YAML_REMOTE_PROCESSING_GROUPS_KEY "Remote Processing Groups"

// Variables that allow us to avoid a timed wait.
sem_t *running;
//! Flow Controller
static std::unique_ptr<minifi::FlowController> controller = nullptr;

/**
 * Removed the stop command from the signal handler so that we could trigger
 * unload after we exit the semaphore controlled critical section in main.
 *
 * Semaphores are a portable choice when using signal handlers. Threads,
 * mutexes, and condition variables are not guaranteed to work within
 * a signal handler. Consequently we will use the semaphore to avoid thread
 * safety issues and.
 */
void sigHandler(int signal) {

  if (signal == SIGINT || signal == SIGTERM) {
    // avoid stopping the controller here.
    sem_post(running);
  }
}

int main(int argc, char **argv) {
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();

  logger->setLogLevel(logging::info);

  uint16_t stop_wait_time = STOP_WAIT_TIME_MS;

  std::string graceful_shutdown_seconds = "";

  running = sem_open("MiNiFiMain", O_CREAT, 0644, 0);
  if (running == SEM_FAILED || running == 0) {

    logger->log_error("could not initialize semaphore");
    perror("initialization failure");
  }
  // assumes POSIX compliant environment
  std::string minifiHome;
  if (const char* env_p = std::getenv(MINIFI_HOME_ENV_KEY)) {
    minifiHome = env_p;
  } else {
    logger->log_info(
        "MINIFI_HOME was not found, determining based on executable path.");
    char *path = NULL;
    char full_path[PATH_MAX];
    path = realpath(argv[0], full_path);
    std::string minifiHomePath(path);
    minifiHomePath = minifiHomePath.substr(0,
                                           minifiHomePath.find_last_of("/\\"));  //Remove /minifi from path
    minifiHome = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));	//Remove /bin from path
  }

  if (signal(SIGINT, sigHandler) == SIG_ERR
      || signal(SIGTERM, sigHandler) == SIG_ERR
      || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    logger->log_error("Can not install signal handler");
    return -1;
  }

  minifi::Configure *configure = minifi::Configure::getConfigure();
  configure->setHome(minifiHome);
  configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  if (configure->get(minifi::Configure::nifi_graceful_shutdown_seconds,
                     graceful_shutdown_seconds)) {
    try {
      stop_wait_time = std::stoi(graceful_shutdown_seconds);
    } catch (const std::out_of_range &e) {
      logger->log_error("%s is out of range. %s",
                        minifi::Configure::nifi_graceful_shutdown_seconds,
                        e.what());
    } catch (const std::invalid_argument &e) {
      logger->log_error("%s contains an invalid argument set. %s",
                        minifi::Configure::nifi_graceful_shutdown_seconds,
                        e.what());
    }
  } else {
    logger->log_debug("%s not set, defaulting to %d",
                      minifi::Configure::nifi_graceful_shutdown_seconds,
                      STOP_WAIT_TIME_MS);
  }

  // set the log configuration.
  std::unique_ptr<logging::BaseLogger> configured_logger =
      logging::LogInstance::getConfiguredLogger(configure);

  logger->updateLogger(std::move(configured_logger));

  // Create repos for flow record and provenance
  std::shared_ptr<provenance::ProvenanceRepository> repo = std::make_shared<
      provenance::ProvenanceRepository>();
  repo->initialize();

  controller = std::unique_ptr<minifi::FlowController>(
      new minifi::FlowControllerImpl(repo));

  // Load flow from specified configuration file
  controller->load();
  // Start Processing the flow

  controller->start();
  logger->log_info("MiNiFi started");

  /**
   * Sem wait provides us the ability to have a controlled
   * yield without the need for a more complex construct and
   * a spin lock
   */
  if (sem_wait(running) != -1)
    perror("sem_wait");

  sem_unlink("MiNiFiMain");

  /**
   * Trigger unload -- wait stop_wait_time
   */
  controller->waitUnload(stop_wait_time);

  logger->log_info("MiNiFi exit");

  return 0;
}
