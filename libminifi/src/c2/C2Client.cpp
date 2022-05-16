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

#include <filesystem>
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
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::c2 {

C2Client::C2Client(
    std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo, std::shared_ptr<core::ContentRepository> content_repo,
    std::unique_ptr<core::FlowConfiguration> flow_configuration, std::shared_ptr<utils::file::FileSystem> filesystem,
    std::function<void()> request_restart, std::shared_ptr<core::logging::Logger> logger)
    : core::Flow(std::move(provenance_repo), std::move(flow_file_repo), std::move(content_repo), std::move(flow_configuration)),
      configuration_(std::move(configuration)),
      filesystem_(std::move(filesystem)),
      logger_(std::move(logger)),
      request_restart_(std::move(request_restart)),
      response_node_manager_(configuration_, provenance_repo_, flow_file_repo_, flow_configuration_.get()) {}

void C2Client::stopC2() {
  if (c2_agent_) {
    c2_agent_->stop();
  }
}

bool C2Client::isC2Enabled() const {
  std::string c2_enable_str;
  configuration_->get(minifi::Configuration::nifi_c2_enable, "c2.enable", c2_enable_str);
  return utils::StringUtils::toBool(c2_enable_str).value_or(false);
}

void C2Client::initialize(core::controller::ControllerServiceProvider *controller, state::Pausable *pause_handler, state::StateMonitor* update_sink) {
  if (!isC2Enabled()) {
    return;
  }

  if (!configuration_->getAgentClass()) {
    logger_->log_info("Agent class is not predefined");
  }

  // Set a persistent fallback agent id. This is needed so that the C2 server can identify the same agent after a restart, even if nifi.c2.agent.identifier is not specified.
  if (auto id = configuration_->get(Configuration::nifi_c2_agent_identifier_fallback)) {
    configuration_->setFallbackAgentIdentifier(*id);
  } else {
    const auto agent_id = getControllerUUID().to_string();
    configuration_->setFallbackAgentIdentifier(agent_id);
    configuration_->set(Configuration::nifi_c2_agent_identifier_fallback, agent_id, PropertyChangeLifetime::PERSISTENT);
  }

  {
    std::lock_guard<std::mutex> lock(initialization_mutex_);
    if (initialized_ && !flow_update_) {
      return;
    }
  }

  // root_response_nodes_ was not cleared before, it is unclear if that was intentional

  std::map<std::string, Connection*> connections;
  if (root_ != nullptr) {
    root_->getConnections(connections);
  }

  std::string class_csv;
  if (configuration_->get(minifi::Configuration::nifi_c2_root_classes, class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (const std::string& clazz : classes) {
      auto response_node = response_node_manager_.loadResponseNode(clazz, root_.get());
      if (!response_node) {
        continue;
      }
      std::lock_guard<std::mutex> guard{metrics_mutex_};
      root_response_nodes_[response_node->getName()] = std::move(response_node);
    }
  }

  loadC2ResponseConfiguration(Configuration::nifi_c2_root_class_definitions);

  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (!initialized_) {
    // C2Agent is initialized once, meaning that a C2-triggered flow/configuration update
    // might not be equal to a fresh restart
    c2_agent_ = std::make_unique<c2::C2Agent>(controller, pause_handler, update_sink, configuration_, filesystem_, request_restart_);
    c2_agent_->start();
    initialized_ = true;
  }
}

std::optional<std::string> C2Client::fetchFlow(const std::string& uri) const {
  if (!c2_agent_) {
    return {};
  }
  return c2_agent_->fetchFlow(uri);
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
          auto response_node = response_node_manager_.loadResponseNode(clazz, root_.get());
          if (!response_node) {
            continue;
          }
          std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(response_node);
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
          if (node != nullptr) {
            std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(node);
          }
        }
      } else {
        if (configuration_->get(classOption, class_definitions)) {
          std::vector<std::string> classes = utils::StringUtils::split(class_definitions, ",");
          for (const std::string& clazz : classes) {
            auto response_node = response_node_manager_.loadResponseNode(clazz, root_.get());
            if (!response_node) {
              continue;
            }
            std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(response_node);
          }

          if (!new_node->isEmpty()) {
            std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(new_node);
          }
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
    return response_node_manager_.getComponentMetricsNode(metrics_class);
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
  configuration_->get(minifi::Configuration::nifi_c2_full_heartbeat, fullHb);
  const bool include = include_manifest || fullHb == "true";

  std::vector<std::shared_ptr<state::response::ResponseNode>> nodes;
  nodes.reserve(root_response_nodes_.size());
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

}  // namespace org::apache::nifi::minifi::c2
