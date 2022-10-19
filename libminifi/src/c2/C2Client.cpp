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
#include "properties/Configuration.h"
#include "utils/file/FileSystem.h"
#include "utils/file/FileUtils.h"
#include "utils/gsl.h"
#include "utils/StringUtils.h"

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
      response_node_loader_(configuration_, provenance_repo_, flow_file_repo_, flow_configuration_.get()) {}

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

  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_ && !flow_update_) {
    return;
  }

  if (!initialized_) {
    initializeResponseNodes(root_.get());
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

void C2Client::loadNodeClasses(const std::string& class_definitions, const std::shared_ptr<state::response::ResponseNode>& new_node) {
  auto classes = utils::StringUtils::split(class_definitions, ",");
  for (const std::string& clazz : classes) {
    auto response_nodes = response_node_loader_.loadResponseNodes(clazz, root_.get());
    if (response_nodes.empty()) {
      continue;
    }
    for (const auto& response_node : response_nodes) {
      std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(response_node);
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
      std::string option = utils::StringUtils::join_pack(prefix, ".", metricsClass);
      std::string classOption = option + ".classes";
      std::string nameOption = option + ".name";

      std::string name;
      if (!configuration_->get(nameOption, name)) {
        continue;
      }
      std::shared_ptr<state::response::ResponseNode> new_node = std::make_shared<state::response::ObjectNode>(name);
      if (configuration_->get(classOption, class_definitions)) {
        loadNodeClasses(class_definitions, new_node);
      } else {
        std::string optionName = utils::StringUtils::join_pack(option, ".", name);
        loadC2ResponseConfiguration(optionName, new_node);
      }

      // We don't need to lock here, we already do it in the initializeResponseNodes member function
      root_response_nodes_[name].push_back(new_node);
    } catch (const std::exception& ex) {
      logger_->log_error("Could not create metrics class %s, exception type: %s, what: %s", metricsClass, typeid(ex).name(), ex.what());
    } catch (...) {
      logger_->log_error("Could not create metrics class %s, exception type: %s", metricsClass, getCurrentExceptionTypeName());
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
      std::string option = utils::StringUtils::join_pack(prefix, ".", metricsClass);
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
          loadNodeClasses(class_definitions, new_node);
          if (!new_node->isEmpty()) {
            std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(new_node);
          }
        } else {
          std::string optionName = utils::StringUtils::join_pack(option, ".", name);
          auto sub_node = loadC2ResponseConfiguration(optionName, new_node);
          std::static_pointer_cast<state::response::ObjectNode>(prev_node)->add_node(sub_node);
        }
      }
    } catch (const std::exception& ex) {
      logger_->log_error("Could not create metrics class %s, exception type: %s, what: %s", metricsClass, typeid(ex).name(), ex.what());
    } catch (...) {
      logger_->log_error("Could not create metrics class %s, exception type: %s", metricsClass, getCurrentExceptionTypeName());
    }
  }
  return prev_node;
}

std::optional<state::response::NodeReporter::ReportedNode> C2Client::getMetricsNode(const std::string& metrics_class) const {
  std::lock_guard<std::mutex> guard{metrics_mutex_};
  const auto createReportedNode = [](const std::vector<std::shared_ptr<state::response::ResponseNode>>& nodes) {
    gsl_Expects(!nodes.empty());
    state::response::NodeReporter::ReportedNode reported_node;
    reported_node.is_array = nodes[0]->isArray();
    reported_node.name = nodes[0]->getName();
    reported_node.serialized_nodes = state::response::ResponseNode::serializeAndMergeResponseNodes(nodes);
    return reported_node;
  };

  if (!metrics_class.empty()) {
    auto metrics_nodes = response_node_loader_.getComponentMetricsNodes(metrics_class);
    if (!metrics_nodes.empty()) {
      return createReportedNode(metrics_nodes);
    }
  } else {
    const auto metrics_it = root_response_nodes_.find("metrics");
    if (metrics_it != root_response_nodes_.end()) {
      return createReportedNode(metrics_it->second);
    }
  }
  return std::nullopt;
}

std::vector<state::response::NodeReporter::ReportedNode> C2Client::getHeartbeatNodes(bool include_manifest) const {
  std::string fullHb{"true"};
  configuration_->get(minifi::Configuration::nifi_c2_full_heartbeat, fullHb);
  const bool include = include_manifest || fullHb == "true";

  std::vector<state::response::NodeReporter::ReportedNode> reported_nodes;
  std::lock_guard<std::mutex> guard{metrics_mutex_};
  reported_nodes.reserve(root_response_nodes_.size());
  for (const auto& [name, node_values] : root_response_nodes_) {
    for (const auto& node : node_values) {
      auto identifier = std::dynamic_pointer_cast<state::response::AgentIdentifier>(node);
      if (identifier) {
        identifier->includeAgentManifest(include);
      }
      if (node) {
        state::response::NodeReporter::ReportedNode reported_node;
        reported_node.name = node->getName();
        reported_node.is_array = node->isArray();
        reported_node.serialized_nodes = node->serialize();
        reported_nodes.push_back(reported_node);
      }
    }
  }
  return reported_nodes;
}

void C2Client::initializeResponseNodes(core::ProcessGroup* root) {
  if (!root_response_nodes_.empty()) {
    return;
  }
  std::string class_csv;
  std::lock_guard<std::mutex> guard{metrics_mutex_};
  if (configuration_->get(minifi::Configuration::nifi_c2_root_classes, class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (const std::string& clazz : classes) {
      auto response_nodes = response_node_loader_.loadResponseNodes(clazz, root);
      if (response_nodes.empty()) {
        continue;
      }

      for (auto&& response_node: response_nodes) {
        root_response_nodes_[response_node->getName()].push_back(std::move(response_node));
      }
    }
  }

  loadC2ResponseConfiguration(Configuration::nifi_c2_root_class_definitions);
}

void C2Client::clearResponseNodes() {
  std::lock_guard<std::mutex> guard{metrics_mutex_};
  root_response_nodes_.clear();
}

}  // namespace org::apache::nifi::minifi::c2
