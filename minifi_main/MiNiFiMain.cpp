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
#include <direct.h>

#endif

#include <fcntl.h>
#include <semaphore.h>
#include <sodium.h>
#include <cstdio>
#include <csignal>

#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "AgentDocs.h"
#include "DiskSpaceWatchdog.h"
#include "Defaults.h"
#include "Fips.h"
#include "FlowController.h"
#include "MainHelper.h"
#include "ResourceClaim.h"
#include "agent/JsonSchema.h"
#include "minifi-cpp/agent/agent_version.h"
#include "argparse/argparse.hpp"
#include "core/BulletinStore.h"
#include "core/ConfigurationFactory.h"
#include "core/Core.h"
#include "core/FlowConfiguration.h"
#include "core/RepositoryFactory.h"
#include "core/extension/ExtensionManager.h"
#include "core/repository/VolatileContentRepository.h"
#include "core/state/MetricsPublisherStore.h"
#include "properties/Decryptor.h"
#include "utils/Environment.h"
#include "utils/FileMutex.h"
#include "utils/file/AssetManager.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "range/v3/algorithm/min_element.hpp"
#include "core/Repository.h"

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
 * a signal handler. Consequently, we will use the semaphore to avoid thread
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

void dumpDocs(const std::string& dir) {
  minifi::docs::AgentDocs docsCreator;

  docsCreator.generate(dir);
}

void writeJsonSchema(std::ostream& out) {
  out << minifi::docs::generateJsonSchema() << '\n';
}

void overridePropertiesFromCommandLine(const argparse::ArgumentParser& parser, const std::shared_ptr<minifi::Configure>& configure) {
  const auto& properties = parser.get<std::vector<std::string>>("--property");
  for (const auto& property : properties) {
    auto property_key_and_value = utils::string::splitAndTrimRemovingEmpty(property, "=");
    if (property_key_and_value.size() != 2) {
      std::cerr << "Command line property must be defined in <key>=<value> format, invalid property: " << property << std::endl;
      std::cerr << parser;
      std::exit(1);
    }
    configure->set(property_key_and_value[0], property_key_and_value[1]);
  }
}

[[nodiscard]] std::optional<int /* exit code */> dumpDocsIfRequested(const argparse::ArgumentParser& parser) {
  if (!parser.is_used("--docs")) {
    return std::nullopt;  // don't exit
  }
  const auto& docs_params = parser.get<std::vector<std::string>>("--docs");
  if (utils::file::create_dir(docs_params[0]) != 0) {
    std::cerr << "Working directory doesn't exist and cannot be created: " << docs_params[0] << std::endl;
    return 1;
  }

  std::cout << "Dumping docs to " << docs_params[0] << std::endl;
  dumpDocs(docs_params[0]);
  return 0;
}

[[nodiscard]] std::optional<int /* exit code */> writeSchemaIfRequested(const argparse::ArgumentParser& parser) {
  if (!parser.is_used("--schema")) {
    return std::nullopt;
  }
  const auto& schema_path = parser.get("--schema");
  if (schema_path == "-") {
    writeJsonSchema(std::cout);
    return 0;
  }

  std::cout << "Writing json schema to " << schema_path << std::endl;
  {
    std::ofstream schema_file{schema_path};
    if (!schema_file) {
      std::cerr << "Failed to open file \"" << schema_path << "\" for writing: " << std::system_category().default_error_condition(errno).message()
                << "\n";
      return 1;
    }
    writeJsonSchema(schema_file);
  }
  return 0;
}

int main(int argc, char **argv) {
  argparse::ArgumentParser argument_parser("Apache MiNiFi C++", minifi::AgentBuild::VERSION);
  argument_parser.add_argument("-p", "--property")
    .append()
    .metavar("KEY=VALUE")
    .help("Override a property read from minifi.properties file in key=value format");
  argument_parser.add_argument("-d", "--docs")
    .nargs(1)
    .metavar("DIRECTORY")
    .help("Generate documentation in the specified directory");
  argument_parser.add_argument("-s", "--schema")
    .metavar("PATH")
    .default_value("-")
    .help("Generate JSON schema to the specified path");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << argument_parser;
    std::exit(1);
  }

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
    minifi::setSyslogLogger();
  }
  auto& logger_configuration = core::logging::LoggerConfiguration::getConfiguration();
  const auto logger = logger_configuration.getLogger("main");
  auto startup_timepoint = std::chrono::system_clock::now();
  auto log_runtime = gsl::finally([&]() {
    logger->log_info("Runtime was {}", utils::timeutils::humanReadableDuration(std::chrono::system_clock::now()-startup_timepoint));
  });
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
  const auto locations = minifi::determineLocations(logger);
  if (!locations) {
    // determineLocations already logged everything we need
    return -1;
  }
  minifi::utils::Environment::setEnvironmentVariable(std::string(MINIFI_HOME_ENV_KEY).c_str(), locations->working_dir_.string().c_str());
  logger->log_info("MiNiFi Locations={}", *locations);

  utils::FileMutex minifi_home_mtx(locations->lock_path_);
  std::unique_lock minifi_home_lock(minifi_home_mtx, std::defer_lock);
  try {
    minifi_home_lock.lock();
  } catch (const std::exception& ex) {
    logger->log_error("Could not acquire LOCK '{}', maybe another minifi instance is running: {}", locations->lock_path_, ex.what());
    std::exit(1);
  }
  // chdir to MINIFI_HOME
  std::error_code current_path_error;
  std::filesystem::current_path(locations->working_dir_, current_path_error);
  if (current_path_error) {
    logger->log_error("Failed to change working directory to {}", locations->working_dir_);
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

    std::string prov_repo_class = "provenancerepository";
    std::string flow_repo_class = "flowfilerepository";
    std::string nifi_configuration_class_name = "adaptiveconfiguration";
    std::string content_repo_class = "DatabaseContentRepository";

    auto log_properties = std::make_shared<core::logging::LoggerProperties>(locations->logs_dir_);
    log_properties->loadConfigureFile(locations->log_properties_path_, "nifi.log.");

    logger_configuration.initialize(log_properties);

    std::shared_ptr<minifi::Properties> uid_properties = std::make_shared<minifi::PropertiesImpl>(minifi::PropertiesImpl::PersistTo::MultipleFiles, "UID properties");
    uid_properties->loadConfigureFile(locations->uid_properties_path_);
    utils::IdGenerator::getIdGenerator()->initialize(uid_properties);

    auto decryptor = minifi::Decryptor::create(locations->working_dir_);
    if (decryptor) {
      logger->log_info("Found encryption key, will decrypt sensitive properties in the configuration");
    } else {
      logger->log_info("No encryption key found, will not decrypt sensitive properties in the configuration");
    }

    const std::shared_ptr<minifi::Configure> configure = std::make_shared<minifi::ConfigureImpl>(std::move(decryptor), std::move(log_properties));
    configure->loadConfigureFile(locations->properties_path_);
    overridePropertiesFromCommandLine(argument_parser, configure);

    minifi::fips::initializeFipsMode(configure, *locations, logger);

    minifi::core::extension::ExtensionManager extension_manager(configure);

    if (const auto maybe_exit_code = dumpDocsIfRequested(argument_parser)) { return *maybe_exit_code; }
    if (const auto maybe_exit_code = writeSchemaIfRequested(argument_parser)) { return *maybe_exit_code; }

    std::chrono::milliseconds stop_wait_time = configure->get(minifi::Configure::nifi_graceful_shutdown_seconds)
        | utils::andThen(utils::timeutils::StringToDuration<std::chrono::milliseconds>)
        | utils::valueOrElse([] { return std::chrono::milliseconds(STOP_WAIT_TIME_MS);});

    configure->get(minifi::Configure::nifi_provenance_repository_class_name, prov_repo_class);
    // Create repos for flow record and provenance
    std::shared_ptr prov_repo = core::createRepository(prov_repo_class, "provenance");

    if (!prov_repo || !prov_repo->initialize(configure)) {
      logger->log_error("Provenance repository failed to initialize, exiting..");
      std::exit(1);
    }

    configure->get(minifi::Configure::nifi_flow_repository_class_name, flow_repo_class);

    std::shared_ptr flow_repo = core::createRepository(flow_repo_class, "flowfile");

    if (!flow_repo || !flow_repo->initialize(configure)) {
      logger->log_error("Flow file repository failed to initialize, exiting..");
      std::exit(1);
    }

    configure->get(minifi::Configure::nifi_content_repository_class_name, content_repo_class);

    std::shared_ptr<core::ContentRepository> content_repo = core::createContentRepository(content_repo_class, "content", *logger);

    if (!content_repo->initialize(configure)) {
      logger->log_error("Content repository failed to initialize, exiting..");
      std::exit(1);
    }
    const bool is_flow_repo_non_persistent = flow_repo->isNoop();
    const bool is_content_repo_non_persistent = std::dynamic_pointer_cast<core::repository::VolatileContentRepository>(content_repo) != nullptr;
    if (is_flow_repo_non_persistent != is_content_repo_non_persistent) {
      logger->log_error("Both or neither of flowfile and content repositories must be persistent! Exiting..");
      std::exit(1);
    }

    std::string content_repo_path;
    if (configure->get(minifi::Configure::nifi_dbcontent_repository_directory_default, content_repo_path) && !content_repo_path.empty()) {
      logger->log_info("setting default dir to {}", content_repo_path);
      minifi::setDefaultDirectory(content_repo_path);
    }

    configure->get(minifi::Configure::nifi_configuration_class_name, nifi_configuration_class_name);

    bool should_encrypt_flow_config = (configure->get(minifi::Configure::nifi_flow_configuration_encrypt)
        | utils::andThen(utils::string::toBool)).value_or(false);

    auto filesystem = std::make_shared<utils::file::FileSystem>(
        should_encrypt_flow_config,
        utils::crypto::EncryptionProvider::create(locations->working_dir_));

    auto asset_manager = std::make_unique<utils::file::AssetManager>(*configure);
    auto bulletin_store = std::make_unique<core::BulletinStore>(*configure);

    std::shared_ptr<core::FlowConfiguration> flow_configuration = core::createFlowConfiguration(
        core::ConfigurationContext{
          .flow_file_repo = flow_repo,
          .content_repo = content_repo,
          .configuration = configure,
          .path = configure->get(minifi::Configure::nifi_flow_configuration_file),
          .filesystem = filesystem,
          .sensitive_values_encryptor = utils::crypto::EncryptionProvider::createSensitivePropertiesEncryptor(locations->working_dir_),
          .asset_manager = asset_manager.get(),
          .bulletin_store = bulletin_store.get()
      }, nifi_configuration_class_name);

    std::vector<std::shared_ptr<core::RepositoryMetricsSource>> repo_metric_sources{prov_repo, flow_repo, content_repo};
    auto metrics_publisher_store = std::make_unique<minifi::state::MetricsPublisherStore>(configure, repo_metric_sources, flow_configuration, asset_manager.get(), bulletin_store.get());
    const auto controller = std::make_unique<minifi::FlowController>(
        prov_repo, flow_repo, configure, std::move(flow_configuration), content_repo,
        std::move(metrics_publisher_store), filesystem, request_restart, asset_manager.get(), bulletin_store.get());

    const bool disk_space_watchdog_enable = configure->get(minifi::Configure::minifi_disk_space_watchdog_enable)
        | utils::andThen(utils::string::toBool)
        | utils::valueOrElse([] { return true; });

    std::unique_ptr<utils::CallBackTimer> disk_space_watchdog;
    if (disk_space_watchdog_enable) {
      try {
        const auto repo_paths = [&] {
          std::vector<std::string> paths;
          paths.reserve(3);
          // REPOSITORY_DIRECTORY is a dummy path used by noop repositories
          const auto path_valid = [](const std::string& p) { return !p.empty() && p != org::apache::nifi::minifi::core::REPOSITORY_DIRECTORY; };
          auto prov_repo_path = prov_repo->getDirectory();
          auto flow_repo_path = flow_repo->getDirectory();
          auto content_repo_storage_path = content_repo->getStoragePath();
          if (!prov_repo->isNoop() && path_valid(prov_repo_path)) { paths.push_back(std::move(prov_repo_path)); }
          if (!flow_repo->isNoop() && path_valid(flow_repo_path)) { paths.push_back(std::move(flow_repo_path)); }
          if (path_valid(content_repo_storage_path)) { paths.push_back(std::move(content_repo_storage_path)); }
          return paths;
        }();
        const auto available_spaces = minifi::disk_space_watchdog::check_available_space(repo_paths, logger.get());
        const auto config = minifi::disk_space_watchdog::read_config(*configure);
        const auto min_space = [](const std::vector<std::uintmax_t>& spaces) {
          const auto it = std::ranges::min_element(spaces);
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
        logger->log_error("{}", error.what());
        return -1;
      }
    }

    logger->log_info("Loading FlowController");

    // Load flow from specified configuration file
    try {
      controller->load();
    } catch (std::exception& e) {
      logger->log_error("Failed to load configuration due to exception: {}", e.what());
      return -1;
    } catch (...) {
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
