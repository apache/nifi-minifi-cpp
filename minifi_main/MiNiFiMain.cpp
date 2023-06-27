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


#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include "MiNiFiWindowsService.h"
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")
#ifdef ENABLE_JNI
  #pragma comment(lib, "jvm.lib")
#endif
#include <direct.h>

#endif

#include <fcntl.h>
#include <cstdio>
#include <semaphore.h>
#include <csignal>
#include <sodium.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "ResourceClaim.h"
#include "core/Core.h"
#include "core/FlowConfiguration.h"
#include "core/ConfigurationFactory.h"
#include "core/RepositoryFactory.h"
#include "core/extension/ExtensionManager.h"
#include "core/repository/VolatileContentRepository.h"
#include "core/repository/VolatileFlowFileRepository.h"
#include "DiskSpaceWatchdog.h"
#include "properties/Decryptor.h"
#include "utils/file/PathUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/Environment.h"
#include "utils/FileMutex.h"
#include "FlowController.h"
#include "AgentDocs.h"
#include "MainHelper.h"
#include "agent/JsonSchema.h"
#include "core/state/nodes/ResponseNodeLoader.h"
#include "c2/C2Agent.h"
#include "core/state/MetricsPublisherFactory.h"
#include "core/state/MetricsPublisherStore.h"

namespace minifi = org::apache::nifi::minifi;
namespace core = minifi::core;
namespace utils = minifi::utils;

static std::atomic_flag flow_controller_running;
static std::atomic_flag process_running;

/**
 * Removed the stop command from the signal handler so that we could trigger
 * unload after we exit the semaphore controlled critical section in main.
 *
 * Semaphores are a portable choice when using signal handlers. Threads,
 * mutexes, and condition variables are not guaranteed to work within
 * a signal handler. Consequently we will use the semaphore to avoid thread
 * safety issues.
 */

#ifdef WIN32
BOOL WINAPI consoleSignalHandler(DWORD signal) {
  if (!process_running.test()) { exit(0); return TRUE; }
  if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
    flow_controller_running.clear();
    flow_controller_running.notify_all();
    process_running.wait(true);
    return TRUE;
  }
  return FALSE;
}

void SignalExitProcess() {
  flow_controller_running.clear();
  flow_controller_running.notify_all();
}
#endif

void sigHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    flow_controller_running.clear();
    flow_controller_running.notify_all();
  }
}

void dumpDocs(const std::shared_ptr<minifi::Configure> &configuration, const std::string &dir, std::ostream &out) {
  auto pythoncreator = core::ClassLoader::getDefaultClassLoader().instantiate("PythonCreator", "PythonCreator");
  if (nullptr != pythoncreator) {
    pythoncreator->configure(configuration);
  }

  minifi::docs::AgentDocs docsCreator;

  docsCreator.generate(dir, out);
}

void writeJsonSchema(const std::shared_ptr<minifi::Configure> &configuration, std::ostream& out) {
  auto pythoncreator = core::ClassLoader::getDefaultClassLoader().instantiate("PythonCreator", "PythonCreator");
  if (nullptr != pythoncreator) {
    pythoncreator->configure(configuration);
  }

  out << minifi::docs::generateJsonSchema();
}

int main(int argc, char **argv) {
#ifdef WIN32
  RunAsServiceIfNeeded();

  auto [isStartedByService, terminationEventHandler] = GetTerminationEventHandle();
  if (isStartedByService && !terminationEventHandler) {
    std::cerr << "Fatal error: started by service, but could not create the termination event handler\n";
    return -1;
  }

  utils::Environment::setRunningAsService(isStartedByService);
#endif

  if (utils::Environment::isRunningAsService()) {
    setSyslogLogger();
  }
  const auto logger = core::logging::LoggerConfiguration::getConfiguration().getLogger("main");

#ifdef WIN32
  if (isStartedByService) {
    if (!CreateServiceTerminationThread(logger, terminationEventHandler)) {
      logger->log_error("Fatal error: started by service, but could not create the service termination thread");
      return -1;
    }
  } else if (terminationEventHandler) {
    CloseHandle(terminationEventHandler);
  }
#endif

  if (sodium_init() < 0) {
    logger->log_error("Could not initialize the libsodium library!");
    return -1;
  }

#ifdef WIN32
  if (!SetConsoleCtrlHandler(consoleSignalHandler, TRUE)) {
    logger->log_error("Cannot install signal handler");
    return -1;
  }

  if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR) {
    logger->log_error("Cannot install signal handler");
    return -1;
  }
#ifdef SIGBREAK
  if (signal(SIGBREAK, sigHandler) == SIG_ERR) {
    logger->log_error("Cannot install signal handler");
    return -1;
  }
#endif
#else
  if (signal(SIGINT, sigHandler) == SIG_ERR || signal(SIGTERM, sigHandler) == SIG_ERR || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    logger->log_error("Cannot install signal handler");
    return -1;
  }
#endif
  // Determine MINIFI_HOME
  const auto minifiHome = determineMinifiHome(logger);
  if (minifiHome.empty()) {
    // determineMinifiHome already logged everything we need
    return -1;
  }
  utils::FileMutex minifi_home_mtx(minifiHome / "LOCK");
  std::unique_lock minifi_home_lock(minifi_home_mtx, std::defer_lock);
  try {
    minifi_home_lock.lock();
  } catch (const std::exception& ex) {
    logger->log_error("Could not acquire LOCK for minifi home '%s', maybe another minifi instance is running: %s", minifiHome.string(), ex.what());
    std::exit(1);
  }
  // chdir to MINIFI_HOME
  std::error_code current_path_error;
  std::filesystem::current_path(minifiHome, current_path_error);
  if (current_path_error) {
    logger->log_error("Failed to change working directory to MINIFI_HOME (%s)", minifiHome.string());
    return -1;
  }

  process_running.test_and_set();

  std::atomic<bool> restart_token{false};
  const auto request_restart = [&] {
    if (!restart_token.exchange(true)) {
      // only trigger if a restart is not already in progress (the flag was unset before the exchange)
      flow_controller_running.clear();
      flow_controller_running.notify_all();
      logger->log_info("Initiating restart...");
    }
  };

  do {
    flow_controller_running.test_and_set();

    std::string graceful_shutdown_seconds;
    std::string prov_repo_class = "provenancerepository";
    std::string flow_repo_class = "flowfilerepository";
    std::string nifi_configuration_class_name = "adaptiveconfiguration";
    std::string content_repo_class = "filesystemrepository";

    auto log_properties = std::make_shared<core::logging::LoggerProperties>();
    log_properties->setHome(minifiHome);
    log_properties->loadConfigureFile(DEFAULT_LOG_PROPERTIES_FILE, "nifi.log.");

    core::logging::LoggerConfiguration::getConfiguration().initialize(log_properties);

    std::shared_ptr<minifi::Properties> uid_properties = std::make_shared<minifi::Properties>("UID properties");
    uid_properties->setHome(minifiHome);
    uid_properties->loadConfigureFile(DEFAULT_UID_PROPERTIES_FILE);
    utils::IdGenerator::getIdGenerator()->initialize(uid_properties);

    // Make a record of minifi home in the configured log file.
    logger->log_info("MINIFI_HOME=%s", minifiHome.string());

    auto decryptor = minifi::Decryptor::create(minifiHome);
    if (decryptor) {
      logger->log_info("Found encryption key, will decrypt sensitive properties in the configuration");
    } else {
      logger->log_info("No encryption key found, will not decrypt sensitive properties in the configuration");
    }

    const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::Configure>(std::move(decryptor), std::move(log_properties));
    configure->setHome(minifiHome);
    configure->loadConfigureFile(DEFAULT_NIFI_PROPERTIES_FILE);

    minifi::core::extension::ExtensionManager::get().initialize(configure);

    if (argc >= 2 && std::string("docs") == argv[1]) {
      if (argc < 3 || argc > 4) {
        std::cerr << "Usage: <minifiexe> docs <directory where to write individual doc files> [file where to write PROCESSORS.md]\n";
        std::cerr << "    If no file name is given for PROCESSORS.md, it will be printed to stdout.\n";
        exit(1);
      }
      if (utils::file::create_dir(argv[2]) != 0) {
        std::cerr << "Working directory doesn't exist and cannot be created: " << argv[2] << std::endl;
        exit(1);
      }

      std::cout << "Dumping docs to " << argv[2] << std::endl;
      if (argc == 4) {
        auto path = std::filesystem::path(argv[3]);
        auto dir = path.parent_path();
        auto filename = path.filename();
        if (dir == argv[2]) {
          std::cerr << "Target file should be out of the working directory: " << dir << std::endl;
          exit(1);
        }
        std::ofstream outref(argv[3]);
        dumpDocs(configure, argv[2], outref);
      } else {
        dumpDocs(configure, argv[2], std::cout);
      }
      exit(0);
    }

    if (argc >= 2 && std::string("schema") == argv[1]) {
      if (argc != 3) {
        std::cerr << "Malformed schema command, expected '<minifiexe> schema <output-file>'" << std::endl;
        std::exit(1);
      }

      std::cout << "Writing json schema to " << argv[2] << std::endl;

      {
        std::ofstream schema_file{argv[2]};
        writeJsonSchema(configure, schema_file);
      }
      std::exit(0);
    }

    std::chrono::milliseconds stop_wait_time = configure->get(minifi::Configure::nifi_graceful_shutdown_seconds)
        | utils::flatMap(utils::timeutils::StringToDuration<std::chrono::milliseconds>)
        | utils::valueOrElse([] { return std::chrono::milliseconds(STOP_WAIT_TIME_MS);});


    configure->get(minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
    // Create repos for flow record and provenance
    std::shared_ptr prov_repo = core::createRepository(prov_repo_class, "provenance");

    if (!prov_repo || !prov_repo->initialize(configure)) {
      logger->log_error("Provenance repository failed to initialize, exiting..");
      exit(1);
    }

    configure->get(minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

    std::shared_ptr flow_repo = core::createRepository(flow_repo_class, "flowfile");

    if (!flow_repo || !flow_repo->initialize(configure)) {
      logger->log_error("Flow file repository failed to initialize, exiting..");
      exit(1);
    }

    configure->get(minifi::Configure::nifi_content_repository_class_name, content_repo_class);

    std::shared_ptr<core::ContentRepository> content_repo = core::createContentRepository(content_repo_class, true, "content");

    if (!content_repo->initialize(configure)) {
      logger->log_error("Content repository failed to initialize, exiting..");
      exit(1);
    }
    const bool is_flow_repo_non_persistent = flow_repo->isNoop() || std::dynamic_pointer_cast<core::repository::VolatileFlowFileRepository>(flow_repo) != nullptr;
    const bool is_content_repo_non_persistent = std::dynamic_pointer_cast<core::repository::VolatileContentRepository>(content_repo) != nullptr;
    if (is_flow_repo_non_persistent != is_content_repo_non_persistent) {
      logger->log_error("Both or neither of flowfile and content repositories must be persistent! Exiting..");
      exit(1);
    }

    std::string content_repo_path;
    if (configure->get(minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path) && !content_repo_path.empty()) {
      core::logging::LOG_INFO(logger) << "setting default dir to " << content_repo_path;
      minifi::setDefaultDirectory(content_repo_path);
    }

    configure->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

    bool should_encrypt_flow_config = (configure->get(minifi::Configure::nifi_flow_configuration_encrypt)
        | utils::flatMap(utils::StringUtils::toBool)).value_or(false);

    auto filesystem = std::make_shared<utils::file::FileSystem>(
        should_encrypt_flow_config,
        utils::crypto::EncryptionProvider::create(minifiHome));

    std::shared_ptr<core::FlowConfiguration> flow_configuration = core::createFlowConfiguration(
        core::ConfigurationContext{
          .flow_file_repo = flow_repo,
          .content_repo = content_repo,
          .configuration = configure,
          .path = configure->get(minifi::Configure::nifi_flow_configuration_file),
          .filesystem = filesystem}, nifi_configuration_class_name);

    std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repo_metric_sources{prov_repo, flow_repo, content_repo};
    auto metrics_publisher_store = std::make_unique<minifi::state::MetricsPublisherStore>(configure, repo_metric_sources, flow_configuration);

    const auto controller = std::make_unique<minifi::FlowController>(
      prov_repo, flow_repo, configure, std::move(flow_configuration), content_repo, std::move(metrics_publisher_store), filesystem, request_restart);

    const bool disk_space_watchdog_enable = configure->get(minifi::Configure::minifi_disk_space_watchdog_enable)
        | utils::flatMap(utils::StringUtils::toBool)
        | utils::valueOrElse([] { return true; });

    std::unique_ptr<utils::CallBackTimer> disk_space_watchdog;
    if (disk_space_watchdog_enable) {
      try {
        const auto repo_paths = [&] {
          std::vector<std::string> repo_paths;
          repo_paths.reserve(3);
          // REPOSITORY_DIRECTORY is a dummy path used by noop repositories
          const auto path_valid = [](const std::string& p) { return !p.empty() && p != org::apache::nifi::minifi::core::REPOSITORY_DIRECTORY; };
          auto prov_repo_path = prov_repo->getDirectory();
          auto flow_repo_path = flow_repo->getDirectory();
          auto content_repo_storage_path = content_repo->getStoragePath();
          if (!prov_repo->isNoop() && path_valid(prov_repo_path)) { repo_paths.push_back(std::move(prov_repo_path)); }
          if (!flow_repo->isNoop() && path_valid(flow_repo_path)) { repo_paths.push_back(std::move(flow_repo_path)); }
          if (path_valid(content_repo_storage_path)) { repo_paths.push_back(std::move(content_repo_storage_path)); }
          return repo_paths;
        }();
        const auto available_spaces = minifi::disk_space_watchdog::check_available_space(repo_paths, logger.get());
        const auto config = minifi::disk_space_watchdog::read_config(*configure);
        const auto min_space = [](const std::vector<std::uintmax_t>& spaces) {
          const auto it = std::min_element(std::begin(spaces), std::end(spaces));
          return it != spaces.end() ? *it : (std::numeric_limits<std::uintmax_t>::max)();
        };
        if (min_space(available_spaces) <= config.stop_threshold_bytes) {
          logger->log_error("Cannot start MiNiFi due to insufficient available disk space");
          return -1;
        }
        auto interval_switch = minifi::disk_space_watchdog::disk_space_interval_switch(config);
        disk_space_watchdog = std::make_unique<utils::CallBackTimer>(config.interval, [interval_switch, min_space, repo_paths, logger, &controller]() mutable {
          const auto stop = [&] {
            controller->stop();
          };
          const auto restart = [&] {
            controller->load();
            controller->start();
          };
          const auto switch_state = interval_switch(min_space(minifi::disk_space_watchdog::check_available_space(repo_paths, logger.get())));
          if (switch_state.state == utils::IntervalSwitchState::LOWER && switch_state.switched) {
            logger->log_warn("Stopping flow controller due to insufficient disk space");
            stop();
          } else if (switch_state.state == utils::IntervalSwitchState::UPPER && switch_state.switched) {
            logger->log_info("Restarting flow controller");
            restart();
          }
        });
      } catch (const std::runtime_error& error) {
        logger->log_error(error.what());
        return -1;
      }
    }

    logger->log_info("Loading FlowController");

    // Load flow from specified configuration file
    try {
      controller->load();
    }
    catch (std::exception& e) {
      logger->log_error("Failed to load configuration due to exception: %s", e.what());
      return -1;
    }
    catch (...) {
      logger->log_error("Failed to load configuration due to unknown exception");
      return -1;
    }

    // Start Processing the flow
    controller->start();

    if (disk_space_watchdog) { disk_space_watchdog->start(); }

    logger->log_info("MiNiFi started");

    flow_controller_running.wait(true);

    disk_space_watchdog = nullptr;

    /**
     * Trigger unload -- wait stop_wait_time
     */
    controller->waitUnload(stop_wait_time);
    flow_repo = nullptr;
    prov_repo = nullptr;
  } while ([&] {
    const auto restart_token_temp = restart_token.exchange(false);
    if (restart_token_temp) {
      logger->log_info("Restarting MiNiFi");
    }
    return restart_token_temp;
  }());

  process_running.clear();
  process_running.notify_all();
  logger->log_info("MiNiFi exit");
  return 0;
}
