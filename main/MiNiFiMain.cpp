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

#include "core/Core.h"

#include "core/FlowConfiguration.h"
#include "core/ConfigurationFactory.h"
#include "core/RepositoryFactory.h"
#include "FlowController.h"

//! Main thread sleep interval 1 second
#define SLEEP_INTERVAL 1
//! Main thread stop wait time
#define STOP_WAIT_TIME_MS 30*1000
//! Default YAML location
#define DEFAULT_NIFI_CONFIG_YML "./conf/config.yml"
//! Default properties file paths
#define DEFAULT_NIFI_PROPERTIES_FILE "./conf/minifi.properties"
#define DEFAULT_LOG_PROPERTIES_FILE "./conf/minifi-log.properties"
#define DEFAULT_UID_PROPERTIES_FILE "./conf/minifi-uid.properties"
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

/**
 * Validates a MINIFI_HOME value.
 * @param home_path
 * @return true if home_path represents a valid MINIFI_HOME
 */
bool validHome(const std::string &home_path) {
  struct stat stat_result{};
  auto properties_file_path = home_path + "/" + DEFAULT_NIFI_PROPERTIES_FILE;
  return (stat(properties_file_path.c_str(), &stat_result) == 0);
}

int main(int argc, char **argv) {
  std::shared_ptr<logging::Logger> logger = logging::LoggerConfiguration::getConfiguration().getLogger("main");

  uint16_t stop_wait_time = STOP_WAIT_TIME_MS;

  // initialize static functions that were defined apriori
  core::FlowConfiguration::initialize_static_functions();

  std::string graceful_shutdown_seconds = "";
  std::string prov_repo_class = "provenancerepository";
  std::string flow_repo_class = "flowfilerepository";
  std::string nifi_configuration_class_name = "yamlconfiguration";
  std::string content_repo_class = "filesystemrepository";

  running = sem_open("MiNiFiMain", O_CREAT, 0644, 0);
  if (running == SEM_FAILED || running == 0) {

    logger->log_error("could not initialize semaphore");
    perror("initialization failure");
  }
  // assumes POSIX compliant environment
  std::string minifiHome;
  if (const char *env_p = std::getenv(MINIFI_HOME_ENV_KEY)) {
    minifiHome = env_p;
    logger->log_info("Using MINIFI_HOME=%s from environment.", minifiHome);
  } else {
    logger->log_info("MINIFI_HOME is not set; determining based on environment.");
    char *path = nullptr;
    char full_path[PATH_MAX];
    path = realpath(argv[0], full_path);

    if (path != nullptr) {
      std::string minifiHomePath(path);
      if (minifiHomePath.find_last_of("/\\") != std::string::npos) {
        minifiHomePath = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));  //Remove /minifi from path
        minifiHome = minifiHomePath.substr(0, minifiHomePath.find_last_of("/\\"));    //Remove /bin from path
      }
    }

    // attempt to use cwd as MINIFI_HOME
    if (minifiHome.empty() || !validHome(minifiHome)) {
      char cwd[PATH_MAX];
      getcwd(cwd, PATH_MAX);
      minifiHome = cwd;
    }
  }

  if (!validHome(minifiHome)) {
    logger->log_error("No valid MINIFI_HOME could be inferred. "
                          "Please set MINIFI_HOME or run minifi from a valid location.");
    return -1;
  }

  if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR
      || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    logger->log_error("Can not install signal handler");
    return -1;
  }

  std::shared_ptr<logging::LoggerProperties> log_properties = std::make_shared<logging::LoggerProperties>();
  log_properties->setHome(minifiHome);
  log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE);
  logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

  std::shared_ptr<minifi::Properties> uid_properties = std::make_shared<minifi::Properties>();
  uid_properties->setHome(minifiHome);
  uid_properties->loadConfigureFile(DEFAULT_UID_PROPERTIES_FILE);
  utils::IdGenerator::getIdGenerator()->initialize(uid_properties);

  // Make a record of minifi home in the configured log file.
  logger->log_info("MINIFI_HOME=%s", minifiHome);

  std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::Configure>();
  configure->setHome(minifiHome);
  configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

  if (configure->get(minifi::Configure::nifi_graceful_shutdown_seconds, graceful_shutdown_seconds)) {
    try {
      stop_wait_time = std::stoi(graceful_shutdown_seconds);
    } catch (const std::out_of_range &e) {
      logger->log_error("%s is out of range. %s", minifi::Configure::nifi_graceful_shutdown_seconds, e.what());
    } catch (const std::invalid_argument &e) {
      logger->log_error("%s contains an invalid argument set. %s",
                        minifi::Configure::nifi_graceful_shutdown_seconds,
                        e.what());
    }
  } else {
    logger->log_debug("%s not set, defaulting to %d", minifi::Configure::nifi_graceful_shutdown_seconds,
                      STOP_WAIT_TIME_MS);
  }

  configure->get(minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
  // Create repos for flow record and provenance
  std::shared_ptr<core::Repository> prov_repo = core::createRepository(prov_repo_class, true, "provenance");
  prov_repo->initialize(configure);

  configure->get(minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

  std::shared_ptr<core::Repository> flow_repo = core::createRepository(flow_repo_class, true, "flowfile");

  flow_repo->initialize(configure);

  configure->get(minifi::Configure::nifi_content_repository_class_name, content_repo_class);

  std::shared_ptr<core::ContentRepository>
      content_repo = core::createContentRepository(content_repo_class, true, "content");

  content_repo->initialize(configure);

  configure->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = std::make_shared<minifi::io::StreamFactory>(configure);

  std::unique_ptr<core::FlowConfiguration> flow_configuration = std::move(core::createFlowConfiguration(prov_repo,
                                                                                                        flow_repo,
                                                                                                        content_repo,
                                                                                                        configure,
                                                                                                        stream_factory,
                                                                                                        nifi_configuration_class_name));

  std::shared_ptr<minifi::FlowController> controller = std::unique_ptr<minifi::FlowController>(
      new minifi::FlowController(prov_repo, flow_repo, configure, std::move(flow_configuration), content_repo));

  logger->log_info("Loading FlowController");

  // Load flow from specified configuration file
  try {
    controller->load();
  } catch (std::exception &e) {
    logger->log_error("Failed to load configuration due to exception: %s", e.what());
    return -1;
  } catch (...) {
    logger->log_error("Failed to load configuration due to unknown exception");
    return -1;
  }

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

  flow_repo = nullptr;

  prov_repo = nullptr;

  logger->log_info("MiNiFi exit");

  return 0;
}
