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
#include "core/state/nodes/ConfigurationChecksums.h"
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

void C2Client::stopC2() {
  if (c2_agent_) {
    c2_agent_->stop();
  }
}

bool C2Client::isC2Enabled() const {
  std::string c2_enable_str;
  configuration_->get(Configure::nifi_c2_enable, "c2.enable", c2_enable_str);
  return utils::StringUtils::toBool(c2_enable_str).value_or(false);
}

void C2Client::initialize(core::controller::ControllerServiceProvider *controller, state::Pausable *pause_handler, const std::shared_ptr<state::StateMonitor> &update_sink) {
  if (!isC2Enabled()) {
    return;
  }

  if (!configuration_->getAgentClass()) {
    logger_->log_info("Agent class is not predefined");
  }

  configuration_->setFallbackAgentIdentifier(getControllerUUID().to_string());

  if (initialized_ && !flow_update_) {
    return;
  }

  // root_response_nodes_ was not cleared before, it is unclear if that was intentional

  std::map<std::string, std::shared_ptr<Connection>> connections;
  if (root_ != nullptr) {
    root_->getConnections(connections);
  }

  std::string class_csv;
  if (configuration_->get("nifi.c2.root.classes", class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (const std::string& clazz : classes) {
      auto response_node = std::dynamic_pointer_cast<state::response::ResponseNode>(core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz));
      if (nullptr == response_node) {
        logger_->log_error("No metric defined for %s", clazz);
        continue;
      }
      auto identifier = std::dynamic_pointer_cast<state::response::AgentIdentifier>(response_node);
      if (identifier != nullptr) {
        identifier->setAgentIdentificationProvider(configuration_);
      }
      auto monitor = std::dynamic_pointer_cast<state::response::AgentMonitor>(response_node);
      if (monitor != nullptr) {
        monitor->addRepository(provenance_repo_);
        monitor->addRepository(flow_file_repo_);
        monitor->setStateMonitor(update_sink);
      }
      auto configuration_checksums = std::dynamic_pointer_cast<state::response::ConfigurationChecksums>(response_node);
      if (configuration_checksums) {
        configuration_checksums->addChecksumCalculator(configuration_->getChecksumCalculator());
        configuration_checksums->addChecksumCalculator(flow_configuration_->getChecksumCalculator());
      }
      auto flowMonitor = std::dynamic_pointer_cast<state::response::FlowMonitor>(response_node);
      if (flowMonitor != nullptr) {
        for (auto &con : connections) {
          flowMonitor->addConnection(con.second);
        }
        flowMonitor->setStateMonitor(update_sink);
        flowMonitor->setFlowVersion(flow_configuration_->getFlowVersion());
      }
      std::lock_guard<std::mutex> guard(metrics_mutex_);
      root_response_nodes_[response_node->getName()] = response_node;
    }
  }

  initializeComponentMetrics();

  loadC2ResponseConfiguration("nifi.c2.root.class.definitions");

  if (!initialized_) {
    // C2Agent is initialized once, meaning that a C2-triggered flow/configuration update
    // might not be equal to a fresh restart
    c2_agent_ = std::unique_ptr<c2::C2Agent>(new c2::C2Agent(controller, pause_handler, update_sink, configuration_, filesystem_));
    c2_agent_->start();
    initialized_ = true;
  }
}

utils::optional<std::string> C2Client::fetchFlow(const std::string& uri) const {
  if (!c2_agent_) {
    return {};
  }
  return c2_agent_->fetchFlow(uri);
}

void C2Client::initializeComponentMetrics() {
  {
    std::lock_guard<std::mutex> guard(metrics_mutex_);
    component_metrics_.clear();
  }

  if (root_ == nullptr) {
    return;
  }
  std::vector<std::shared_ptr<core::Processor>> processors;
  root_->getAllProcessors(processors);
  for (const auto &processor : processors) {
    auto rep = std::dynamic_pointer_cast<state::response::ResponseNodeSource>(processor);
    if (rep == nullptr) {
      continue;
    }
    // we have a metrics source.
    std::vector<std::shared_ptr<state::response::ResponseNode>> metric_vector;
    rep->getResponseNodes(metric_vector);
    std::lock_guard<std::mutex> guard(metrics_mutex_);
    for (auto& metric : metric_vector) {
      component_metrics_[metric->getName()] = metric;
    }
  }
}

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
            const bool found_metric = [&] {
              std::lock_guard<std::mutex> guard{metrics_mutex_};
              auto metric = component_metrics_.find(clazz);
              if (metric != component_metrics_.end()) {
                ptr = metric->second;
                return true;
              }
              return false;
            }();
            if (!found_metric) {
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

      std::lock_guard<std::mutex> guard{metrics_mutex_};
      root_response_nodes_[name] = new_node;
    } catch (...) {
      logger_->log_error("Could not create metrics class %s", metricsClass);
    }
  }
}

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
              const bool found_metric = [&] {
                std::lock_guard<std::mutex> guard{metrics_mutex_};
                auto metric = component_metrics_.find(clazz);
                if (metric != component_metrics_.end()) {
                  ptr = metric->second;
                  return true;
                }
                return false;
              }();
              if (!found_metric) {
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
  if (!metrics_class.empty()) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    const auto citer = component_metrics_.find(metrics_class);
    if (citer != component_metrics_.end()) {
      return citer->second;
    }
  } else {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    const auto iter = root_response_nodes_.find("metrics");
    if (iter != root_response_nodes_.end()) {
      return iter->second;
    }
  }
  return nullptr;
}

std::vector<std::shared_ptr<state::response::ResponseNode>> C2Client::getHeartbeatNodes(bool include_manifest) const {
  std::string fullHb{"true"};
  configuration_->get("nifi.c2.full.heartbeat", fullHb);
  const bool include = include_manifest || fullHb == "true";

  std::vector<std::shared_ptr<state::response::ResponseNode>> nodes;
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  for (const auto &entry : root_response_nodes_) {
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
