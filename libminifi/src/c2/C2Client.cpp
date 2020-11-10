/**
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

#include <memory>
#include <map>
#include "c2/C2Client.h"
#include "core/state/nodes/MetricsBase.h"
#include "core/state/nodes/QueueMetrics.h"
#include "core/state/nodes/AgentInformation.h"
#include "core/state/nodes/RepositoryMetrics.h"
#include "properties/Configure.h"
#include "core/state/UpdateController.h"
#include "core/controller/ControllerServiceProvider.h"
#include "c2/C2Agent.h"
#include "core/state/nodes/FlowInformation.h"
#include "utils/file/FileSystem.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

C2Client::C2Client(
    std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::ContentRepository> content_repo,
    std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<utils::file::FileSystem> filesystem,
    std::shared_ptr<logging::Logger> logger)
    : core::Flow(std::move(provenance_repo), std::move(flow_file_repo), std::move(content_repo), std::move(flow_configuration)),
      configuration_(std::move(configuration)),
      filesystem_(std::move(filesystem)),
      logger_(std::move(logger)) {}

void C2Client::initialize(core::controller::ControllerServiceProvider *controller, const std::shared_ptr<state::StateMonitor> &update_sink) {
  std::lock_guard<std::mutex> lock(metrics_mutex_);

  std::string class_str;
  configuration_->get("nifi.c2.agent.class", "c2.agent.class", class_str);
  configuration_->setAgentClass(class_str);

  std::string c2_enable_str;
  bool c2_enabled = false;
  if (configuration_->get(Configure::nifi_c2_enable, "c2.enable", c2_enable_str)) {
    utils::StringUtils::StringToBool(c2_enable_str, c2_enabled);
  }

  if (!c2_enabled) {
    return;
  }

  if (class_str.empty()) {
    logger_->log_error("Class name must be defined when C2 is enabled");
    throw std::runtime_error("Class name must be defined when C2 is enabled");
  }

  std::string identifier_str;
  if (!configuration_->get("nifi.c2.agent.identifier", "c2.agent.identifier", identifier_str) || identifier_str.empty()) {
    // set to the flow controller's identifier
    identifier_str = getControllerUUID().to_string();
  }
  configuration_->setAgentIdentifier(identifier_str);

  if (initialized_ && !flow_update_) {
    return;
  }

  std::string class_csv;
  if (root_ != nullptr) {
    std::shared_ptr<state::response::QueueMetrics> queueMetrics = std::make_shared<state::response::QueueMetrics>();
    std::map<std::string, std::shared_ptr<Connection>> connections;
    root_->getConnections(connections);
    for (auto& con : connections) {
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

    for (const std::string& clazz : classes) {
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
        monitor->setStateMonitor(update_sink);
      }
      auto flowMonitor = std::dynamic_pointer_cast<state::response::FlowMonitor>(processor);
      std::map<std::string, std::shared_ptr<Connection>> connections;
      if (root_ != nullptr) {
        root_->getConnections(connections);
      }
      if (flowMonitor != nullptr) {
        for (auto &con : connections) {
          flowMonitor->addConnection(con.second);
        }
        flowMonitor->setStateMonitor(update_sink);
        flowMonitor->setFlowVersion(flow_configuration_->getFlowVersion());
      }
      root_response_nodes_[processor->getName()] = processor;
    }
  }

  if (configuration_->get("nifi.flow.metrics.classes", class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");
    for (const std::string& clazz : classes) {
      auto ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);
      if (nullptr == ptr) {
        logger_->log_error("No metric defined for %s", clazz);
        continue;
      }
      std::shared_ptr<state::response::ResponseNode> processor = std::static_pointer_cast<state::response::ResponseNode>(ptr);
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
        for (auto& metric : metric_vector) {
          component_metrics_[metric->getName()] = metric;
        }
      }
    }
  }

  std::string class_definitions;
  if (configuration_->get("nifi.flow.metrics.class.definitions", class_definitions)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

    for (const std::string& metricsClass : classes) {
      try {
        int id = std::stoi(metricsClass);
        std::stringstream option;
        option << "nifi.flow.metrics.class.definitions." << metricsClass;
        if (configuration_->get(option.str(), class_definitions)) {
          std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

          for (const std::string& clazz : classes) {
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

  loadC2ResponseConfiguration("nifi.c2.root.class.definitions");

  if (!initialized_) {
    c2_agent_ = std::unique_ptr<c2::C2Agent>(new c2::C2Agent(controller, update_sink, configuration_, filesystem_));
    c2_agent_->start();
    initialized_ = true;
  }
}

// must hold the metrics_mutex_
void C2Client::loadC2ResponseConfiguration(const std::string &prefix) {
  std::string class_definitions;
  if (!configuration_->get(prefix, class_definitions)) {
    return;
  }

  std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

  for (const std::string& metricsClass : classes) {
    try {
      std::string option = prefix + "." + metricsClass;
      std::string classOption = option + ".classes";
      std::string nameOption = option + ".name";

      std::string name;
      if (!configuration_->get(nameOption, name)) {
        continue;
      }
      std::shared_ptr<state::response::ResponseNode> new_node = std::make_shared<state::response::ObjectNode>(name);
      if (configuration_->get(classOption, class_definitions)) {
        std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");
        for (const std::string& clazz : classes) {
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
        std::string optionName = option + "." + name;
        auto node = loadC2ResponseConfiguration(optionName, new_node);
      }

      root_response_nodes_[name] = new_node;
    } catch (...) {
      logger_->log_error("Could not create metrics class %s", metricsClass);
    }
  }
}

// must hold the metrics_mutex_
std::shared_ptr<state::response::ResponseNode> C2Client::loadC2ResponseConfiguration(const std::string &prefix, std::shared_ptr<state::response::ResponseNode> prev_node) {
  std::string class_definitions;
  if (!configuration_->get(prefix, class_definitions)) {
    return prev_node;
  }
  std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");

  for (const std::string& metricsClass : classes) {
    try {
      std::string option = prefix + "." + metricsClass;
      std::string classOption = option + ".classes";
      std::string nameOption = option + ".name";

      std::string name;
      if (!configuration_->get(nameOption, name)) {
        continue;
      }
      std::shared_ptr<state::response::ResponseNode> new_node = std::make_shared<state::response::ObjectNode>(name);
      if (name.find(',') != std::string::npos) {
        std::vector<std::string> sub_classes = utils::StringUtils::split(name, ",");
        for (const std::string& subClassStr : classes) {
          auto node = loadC2ResponseConfiguration(subClassStr, prev_node);
          if (node != nullptr)
            std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(node);
        }
      } else {
        if (configuration_->get(classOption, class_definitions)) {
          std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");
          for (const std::string& clazz : classes) {
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
          std::string optionName = option + "." + name;
          auto sub_node = loadC2ResponseConfiguration(optionName, new_node);
          std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(sub_node);
        }
      }
    } catch (...) {
      logger_->log_error("Could not create metrics class %s", metricsClass);
    }
  }
  return prev_node;
}

std::shared_ptr<state::response::ResponseNode> C2Client::getMetricsNode(const std::string& metrics_class) const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  if (!metrics_class.empty()) {
    const auto citer = component_metrics_.find(metrics_class);
    if (citer != component_metrics_.end()) {
      return citer->second;
    }
  } else {
    const auto iter = root_response_nodes_.find("metrics");
    if (iter != root_response_nodes_.end()) {
      return iter->second;
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<state::response::ResponseNode>> C2Client::getHeartbeatNodes(bool include_manifest) const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  std::string fullHb{"true"};
  configuration_->get("nifi.c2.full.heartbeat", fullHb);
  const bool include = include_manifest || fullHb == "true";

  std::vector<std::shared_ptr<state::response::ResponseNode>> nodes;
  for (const auto& entry : root_response_nodes_) {
    auto identifier = std::dynamic_pointer_cast<state::response::AgentIdentifier>(entry.second);
    if (identifier) {
      identifier->includeAgentManifest(include);
    }
    nodes.push_back(entry.second);
  }
  return nodes;
}

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
