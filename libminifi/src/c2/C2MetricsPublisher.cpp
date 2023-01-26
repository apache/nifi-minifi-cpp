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
#include "c2/C2MetricsPublisher.h"
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
#include "core/Resource.h"

namespace org::apache::nifi::minifi::c2 {

C2MetricsPublisher::C2MetricsPublisher(const std::string &name, const utils::Identifier &uuid)
  : core::CoreComponent(name, uuid) {}

void C2MetricsPublisher::loadNodeClasses(const std::string& class_definitions, const std::shared_ptr<state::response::ResponseNode>& new_node) {
  gsl_Expects(response_node_loader_);
  auto classes = utils::StringUtils::split(class_definitions, ",");
  for (const std::string& clazz : classes) {
    auto response_nodes = response_node_loader_->loadResponseNodes(clazz);
    if (response_nodes.empty()) {
      continue;
    }
    for (const auto& response_node : response_nodes) {
      std::static_pointer_cast<state::response::ObjectNode>(new_node)->add_node(response_node);
    }
  }
}

void C2MetricsPublisher::loadC2ResponseConfiguration(const std::string &prefix) {
  gsl_Expects(configuration_);
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

      // We don't need to lock here, we already do it in the loadMetricNodes member function
      root_response_nodes_[name].push_back(new_node);
    } catch (const std::exception& ex) {
      logger_->log_error("Could not create metrics class %s, exception type: %s, what: %s", metricsClass, typeid(ex).name(), ex.what());
    } catch (...) {
      logger_->log_error("Could not create metrics class %s, exception type: %s", metricsClass, getCurrentExceptionTypeName());
    }
  }
}

std::shared_ptr<state::response::ResponseNode> C2MetricsPublisher::loadC2ResponseConfiguration(const std::string &prefix,
    std::shared_ptr<state::response::ResponseNode> prev_node) {
  gsl_Expects(configuration_);
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

std::optional<state::response::NodeReporter::ReportedNode> C2MetricsPublisher::getMetricsNode(const std::string& metrics_class) const {
  gsl_Expects(response_node_loader_);
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
    auto metrics_nodes = response_node_loader_->loadResponseNodes(metrics_class);
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

std::vector<state::response::NodeReporter::ReportedNode> C2MetricsPublisher::getHeartbeatNodes(bool include_manifest) const {
  gsl_Expects(configuration_);
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

void C2MetricsPublisher::loadMetricNodes() {
  gsl_Expects(response_node_loader_ && configuration_);
  if (!root_response_nodes_.empty()) {
    return;
  }
  std::string class_csv;
  std::lock_guard<std::mutex> guard{metrics_mutex_};
  if (configuration_->get(minifi::Configuration::nifi_c2_root_classes, class_csv)) {
    std::vector<std::string> classes = utils::StringUtils::split(class_csv, ",");

    for (const std::string& clazz : classes) {
      auto response_nodes = response_node_loader_->loadResponseNodes(clazz);
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

void C2MetricsPublisher::clearMetricNodes() {
  std::lock_guard<std::mutex> guard{metrics_mutex_};
  root_response_nodes_.clear();
}

state::response::NodeReporter::ReportedNode C2MetricsPublisher::getAgentManifest() {
  gsl_Expects(response_node_loader_);
  return response_node_loader_->getAgentManifest();
}

REGISTER_RESOURCE(C2MetricsPublisher, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
