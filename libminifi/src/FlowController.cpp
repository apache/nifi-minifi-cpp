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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
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

#include "core/state/nodes/AgentInformation.h"
#include "core/state/nodes/BuildInformation.h"
#include "core/state/nodes/DeviceInformation.h"
#include "core/state/nodes/FlowInformation.h"
#include "core/state/nodes/ProcessMetrics.h"
#include "core/state/nodes/QueueMetrics.h"
#include "core/state/nodes/RepositoryMetrics.h"
#include "core/state/nodes/SystemMetrics.h"
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
#include "utils/HTTPClient.h"
#include "io/NetworkPrioritizer.h"

#ifdef _MSC_VER
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::shared_ptr<utils::IdGenerator> FlowController::id_generator_ = utils::IdGenerator::getIdGenerator();

#define DEFAULT_CONFIG_NAME "conf/config.yml"

FlowController::FlowController(std::shared_ptr<core::Repository> provenance_repo, std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<Configure> configure,
                               std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<core::ContentRepository> content_repo, const std::string name, bool headless_mode)
    : core::controller::ControllerServiceProvider(core::getClassName<FlowController>()),
      root_(nullptr),
      max_timer_driven_threads_(0),
      max_event_driven_threads_(0),
      running_(false),
      updating_(false),
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

  flow_update_ = false;
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
    initializeExternalComponents();
    initializePaths(adjustedFilename);
  }
}

void FlowController::initializeExternalComponents() {
  auto jvmCreator = core::ClassLoader::getDefaultClassLoader().instantiate("JVMCreator", "JVMCreator");
  if (nullptr != jvmCreator) {
    logger_->log_debug("JVMCreator loaded...");
    jvmCreator->configure(configuration_);
  }

  auto pythoncreator = core::ClassLoader::getDefaultClassLoader().instantiate("PythonCreator", "PythonCreator");
  if (nullptr != pythoncreator) {
    logger_->log_debug("PythonCreator loaded...");
    pythoncreator->configure(configuration_);
  }
}

void FlowController::initializePaths(const std::string &adjustedFilename) {
  char *path = NULL;
#ifndef WIN32
  char full_path[PATH_MAX];
  path = realpath(adjustedFilename.c_str(), full_path);
#else
  path = const_cast<char*>(adjustedFilename.c_str());
#endif

  if (path == NULL) {
    throw std::runtime_error("Path is not specified. Either manually set MINIFI_HOME or ensure ../conf exists");
  }
  std::string pathString(path);
  configuration_filename_ = pathString;
  logger_->log_info("FlowController NiFi Configuration file %s", pathString);

  if (!path) {
    logger_->log_error("Could not locate path from provided configuration file name (%s).  Exiting.", path);
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

bool FlowController::applyConfiguration(const std::string &source, const std::string &configurePayload) {
  std::unique_ptr<core::ProcessGroup> newRoot;
  try {
    newRoot = flow_configuration_->updateFromPayload(source, configurePayload);
  } catch (...) {
    logger_->log_error("Invalid configuration payload");
    return false;
  }

  if (newRoot == nullptr)
    return false;

  logger_->log_info("Starting to reload Flow Controller with flow control name %s, version %d", newRoot->getName(), newRoot->getVersion());

  updating_ = true;

  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  stop(true);
  waitUnload(30000);
  controller_map_->clear();
  auto prevRoot = std::move(this->root_);
  this->root_ = std::move(newRoot);
  initialized_ = false;
  bool started = false;
  try {
    load(this->root_, true);
    flow_update_ = true;
    started = start() == 0;

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
  } catch (...) {
    this->root_ = std::move(prevRoot);
    load(this->root_, true);
    flow_update_ = true;
    updating_ = false;
  }

  return started;
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

void FlowController::load(const std::shared_ptr<core::ProcessGroup> &root, bool reload) {
  std::lock_guard<std::recursive_mutex> flow_lock(mutex_);
  if (running_) {
    stop(true);
  }
  if (!initialized_) {
    if (root) {
      logger_->log_info("Load Flow Controller from provided root");
    } else {
      logger_->log_info("Load Flow Controller from file %s", configuration_filename_.c_str());
    }

    if (reload) {
      io::NetworkPrioritizerFactory::getInstance()->clearPrioritizer();
    }

    this->root_ = root == nullptr ? std::shared_ptr<core::ProcessGroup>(flow_configuration_->getRoot(configuration_filename_)) : root;

    logger_->log_info("Loaded root processor Group");

    logger_->log_info("Initializing timers");

    controller_service_provider_ = flow_configuration_->getControllerServiceProvider();

    if (nullptr == timer_scheduler_ || reload) {
      timer_scheduler_ = std::make_shared<TimerDrivenSchedulingAgent>(
          std::static_pointer_cast<core::controller::ControllerServiceProvider>(std::dynamic_pointer_cast<FlowController>(shared_from_this())), provenance_repo_, flow_file_repo_, content_repo_,
          configuration_);
    }
    if (nullptr == event_scheduler_ || reload) {
      event_scheduler_ = std::make_shared<EventDrivenSchedulingAgent>(
          std::static_pointer_cast<core::controller::ControllerServiceProvider>(std::dynamic_pointer_cast<FlowController>(shared_from_this())), provenance_repo_, flow_file_repo_, content_repo_,
          configuration_);
    }

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
  logger_->log_info("Starting to reload Flow Controller with yaml %s", yamlFile);
  stop(true);
  unload();
  std::string oldYamlFile = this->configuration_filename_;
  this->configuration_filename_ = yamlFile;
  load();
  start();
  if (this->root_ != nullptr) {
    this->configuration_filename_ = oldYamlFile;
    logger_->log_info("Rollback Flow Controller to YAML %s", oldYamlFile);
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

  std::string c2_enable_str;
  std::string class_str;

  // don't need to worry about the return code, only whether class_str is defined.
  configuration_->get("nifi.c2.agent.class", "c2.agent.class", class_str);

  if (configuration_->get(Configure::nifi_c2_enable, "c2.enable", c2_enable_str)) {
    bool enable_c2 = true;
    utils::StringUtils::StringToBool(c2_enable_str, enable_c2);
    c2_enabled_ = enable_c2;
    if (c2_enabled_ && class_str.empty()) {
      logger_->log_error("Class name must be defined when C2 is enabled");
      std::cerr << "Class name must be defined when C2 is enabled" << std::endl;
      stop(true);
      exit(1);
    }
  } else {
    /**
     * To require a C2 agent class we will disable C2 by default. If a registration process
     * is implemented we can re-enable. The reason for always enabling C2 is because this allows the controller
     * mechanism that can be used for local config/access to be used. Without this agent information cannot be
     * gathered even if a remote C2 server is enabled.
     *
     * The ticket that impacts this, MINIFICPP-664, should be reversed in the event that agent registration
     * can be performed and agent classes needn't be defined a priori.
     */
    c2_enabled_ = false;
  }

  if (!c2_enabled_) {
    return;
  }

  std::string identifier_str;
  if (!configuration_->get("nifi.c2.agent.identifier", "c2.agent.identifier", identifier_str) || identifier_str.empty()) {
    // set to the flow controller's identifier
    identifier_str = uuidStr_;
  }

  if (!c2_initialized_) {
    configuration_->setAgentIdentifier(identifier_str);
    state::StateManager::initialize();
    std::shared_ptr<c2::C2Agent> agent = std::make_shared<c2::C2Agent>(std::dynamic_pointer_cast<FlowController>(shared_from_this()), std::dynamic_pointer_cast<FlowController>(shared_from_this()),
                                                                       configuration_);
    registerUpdateListener(agent, agent->getHeartBeatDelay());

    state::StateManager::startMetrics(agent->getHeartBeatDelay());

    c2_initialized_ = true;
  } else {
    if (!flow_update_) {
      return;
    }
  }
  device_information_.clear();
  component_metrics_.clear();
  component_metrics_by_id_.clear();
  std::string class_csv;

  if (root_ != nullptr) {
    std::shared_ptr<state::response::QueueMetrics> queueMetrics = std::make_shared<state::response::QueueMetrics>();

    std::map<std::string, std::shared_ptr<Connection>> connections;
    root_->getConnections(connections);
    for (auto con : connections) {
      queueMetrics->addConnection(con.second);
    }
    device_information_[queueMetrics->getName()] = queueMetrics;

    std::shared_ptr<state::response::RepositoryMetrics> repoMetrics = std::make_shared<state::response::RepositoryMetrics>();

    repoMetrics->addRepository(provenance_repo_);
    repoMetrics->addRepository(flow_file_repo_);

    device_information_[repoMetrics->getName()] = repoMetrics;
  }

  if (configuration_->get("nifi.c2.root.classes", class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (std::string clazz : classes) {
      auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);

      if (nullptr == ptr) {
        logger_->log_error("No metric defined for %s", clazz);
        continue;
      }

      std::shared_ptr<state::response::ResponseNode> processor = std::static_pointer_cast<state::response::ResponseNode>(ptr);

      auto identifier = std::dynamic_pointer_cast<state::response::AgentIdentifier>(processor);

      if (identifier != nullptr) {
        identifier->setIdentifier(identifier_str);

        identifier->setAgentClass(class_str);
      }

      auto monitor = std::dynamic_pointer_cast<state::response::AgentMonitor>(processor);
      if (monitor != nullptr) {
        monitor->addRepository(provenance_repo_);
        monitor->addRepository(flow_file_repo_);
        monitor->setStateMonitor(shared_from_this());
      }

      auto flowMonitor = std::dynamic_pointer_cast<state::response::FlowMonitor>(processor);
      std::map<std::string, std::shared_ptr<Connection>> connections;
      root_->getConnections(connections);
      if (flowMonitor != nullptr) {
        for (auto con : connections) {
          flowMonitor->addConnection(con.second);
        }
        flowMonitor->setStateMonitor(shared_from_this());

        flowMonitor->setFlowVersion(flow_configuration_->getFlowVersion());
      }

      std::lock_guard<std::mutex> lock(metrics_mutex_);

      root_response_nodes_[processor->getName()] = processor;
    }
  }

  if (configuration_->get("nifi.flow.metrics.classes", class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (std::string clazz : classes) {
      auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);

      if (nullptr == ptr) {
        logger_->log_error("No metric defined for %s", clazz);
        continue;
      }

      std::shared_ptr<state::response::ResponseNode> processor = std::static_pointer_cast<state::response::ResponseNode>(ptr);

      std::lock_guard<std::mutex> lock(metrics_mutex_);

      device_information_[processor->getName()] = processor;
    }
  }

  // first we should get all component metrics, then
  // we will build the mapping
  std::vector<std::shared_ptr<core::Processor>> processors;
  if (root_ != nullptr) {
    root_->getAllProcessors(processors);
    for (const auto &processor : processors) {
      auto rep = std::dynamic_pointer_cast<state::response::ResponseNodeSource>(processor);
      // we have a metrics source.
      if (nullptr != rep) {
        std::vector<std::shared_ptr<state::response::ResponseNode>> metric_vector;
        rep->getResponseNodes(metric_vector);
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
              ret = device_information_[clazz];
            }
            if (nullptr == ret) {
              logger_->log_error("No metric defined for %s", clazz);
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

  loadC2ResponseConfiguration();
}

void FlowController::loadC2ResponseConfiguration(const std::string &prefix) {
  std::string class_definitions;

  if (configuration_->get(prefix, class_definitions)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

    for (std::string metricsClass : classes) {
      try {
        std::stringstream option;
        option << prefix << "." << metricsClass;

        std::stringstream classOption;
        classOption << option.str() << ".classes";

        std::stringstream nameOption;
        nameOption << option.str() << ".name";
        std::string name;

        if (configuration_->get(nameOption.str(), name)) {
          std::shared_ptr<state::response::ResponseNode> new_node = std::make_shared<state::response::ObjectNode>(name);

          if (configuration_->get(classOption.str(), class_definitions)) {
            std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

            for (std::string clazz : classes) {
              std::lock_guard<std::mutex> lock(metrics_mutex_);

              // instantiate the object
              auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);

              if (nullptr == ptr) {
                auto metric = component_metrics_.find(clazz);
                if (metric != component_metrics_.end()) {
                  ptr = metric->second;
                } else {
                  logger_->log_error("No metric defined for %s", clazz);
                  continue;
                }
              }

              auto node = std::dynamic_pointer_cast<state::response::ResponseNode>(ptr);

              std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(node);
            }

          } else {
            std::stringstream optionName;
            optionName << option.str() << "." << name;
            auto node = loadC2ResponseConfiguration(optionName.str(), new_node);
//            if (node != nullptr && new_node != node)
            //            std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(node);
          }

          root_response_nodes_[name] = new_node;
        }
      } catch (...) {
        logger_->log_error("Could not create metrics class %s", metricsClass);
      }
    }
  }
}

std::shared_ptr<state::response::ResponseNode> FlowController::loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode> prev_node) {
  std::string class_definitions;
  if (configuration_->get(prefix, class_definitions)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

    for (std::string metricsClass : classes) {
      try {
        std::stringstream option;
        option << prefix << "." << metricsClass;

        std::stringstream classOption;
        classOption << option.str() << ".classes";

        std::stringstream nameOption;
        nameOption << option.str() << ".name";
        std::string name;

        if (configuration_->get(nameOption.str(), name)) {
          std::shared_ptr<state::response::ResponseNode> new_node = std::make_shared<state::response::ObjectNode>(name);
          if (name.find(",") != std::string::npos) {
            std::vector<std::string> sub_classes = utils::StringUtils::split(name, ",");
            for (std::string subClassStr : classes) {
              auto node = loadC2ResponseConfiguration(subClassStr, prev_node);
              if (node != nullptr)
                std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(node);
            }

          } else {
            if (configuration_->get(classOption.str(), class_definitions)) {
              std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

              for (std::string clazz : classes) {
                std::lock_guard<std::mutex> lock(metrics_mutex_);

                // instantiate the object
                auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);

                if (nullptr == ptr) {
                  auto metric = component_metrics_.find(clazz);
                  if (metric != component_metrics_.end()) {
                    ptr = metric->second;
                  } else {
                    logger_->log_error("No metric defined for %s", clazz);
                    continue;
                  }
                }

                auto node = std::dynamic_pointer_cast<state::response::ResponseNode>(ptr);

                std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(node);
              }
              if (!new_node->isEmpty())
                std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(new_node);

            } else {
              std::stringstream optionName;
              optionName << option.str() << "." << name;
              auto sub_node = loadC2ResponseConfiguration(optionName.str(), new_node);
              std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(sub_node);
            }
          }
        }
      } catch (...) {
        logger_->log_error("Could not create metrics class %s", metricsClass);
      }
    }
  }
  return prev_node;
}

void FlowController::loadC2ResponseConfiguration() {
  loadC2ResponseConfiguration("nifi.c2.root.class.definitions");
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
std::shared_ptr<core::controller::ControllerServiceNode> FlowController::createControllerService(const std::string &type, const std::string &fullType, const std::string &id, bool firstTimeAdded) {
  return controller_service_provider_->createControllerService(type, fullType, id, firstTimeAdded);
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
std::future<uint64_t> FlowController::enableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
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
std::future<uint64_t> FlowController::disableControllerService(std::shared_ptr<core::controller::ControllerServiceNode> &serviceNode) {
  return controller_service_provider_->disableControllerService(serviceNode);
}

/**
 * Gets all controller services.
 */
std::vector<std::shared_ptr<core::controller::ControllerServiceNode>> FlowController::getAllControllerServices() {
  return controller_service_provider_->getAllControllerServices();
}

/**
 * Gets the controller service for <code>identifier</code>
 * @param identifier service identifier
 * @return shared pointer to teh controller service implementation or nullptr if it does not exist.
 */
std::shared_ptr<core::controller::ControllerService> FlowController::getControllerService(const std::string &identifier) {
  return controller_service_provider_->getControllerService(identifier);
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

int16_t FlowController::applyUpdate(const std::string &source, const std::string &configuration) {
  if (applyConfiguration(source, configuration)) {
    return 1;
  } else {
    return 0;
  }
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

int16_t FlowController::getResponseNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector, uint16_t metricsClass) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);

  for (auto metric : root_response_nodes_) {
    metric_vector.push_back(metric.second);
  }

  return 0;
}

int16_t FlowController::getMetricsNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector, uint16_t metricsClass) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (metricsClass == 0) {
    for (auto metric : device_information_) {
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

std::vector<BackTrace> FlowController::getTraces() {
  std::vector<BackTrace> traces;
  auto timer_driven = timer_scheduler_->getTraces();
  traces.insert(traces.end(), std::make_move_iterator(timer_driven.begin()), std::make_move_iterator(timer_driven.end()));
  auto event_driven = event_scheduler_->getTraces();
  traces.insert(traces.end(), std::make_move_iterator(event_driven.begin()), std::make_move_iterator(event_driven.end()));
  // repositories
  auto prov_repo_trace = provenance_repo_->getTraces();
  traces.emplace_back(std::move(prov_repo_trace));
  auto flow_repo_trace = flow_file_repo_->getTraces();
  traces.emplace_back(std::move(flow_repo_trace));
  auto my_traces = TraceResolver::getResolver().getBackTrace("main");
  traces.emplace_back(std::move(my_traces));
  return traces;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
