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
#include "core/state/nodes/ResponseNodeLoader.h"

#include <vector>

#include "core/Processor.h"
#include "core/ClassLoader.h"
#include "core/state/nodes/RepositoryMetrics.h"
#include "core/state/nodes/QueueMetrics.h"
#include "core/state/nodes/AgentInformation.h"
#include "core/state/nodes/ConfigurationChecksums.h"
#include "c2/C2Agent.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::state::response {

ResponseNodeLoader::ResponseNodeLoader(std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo, core::FlowConfiguration* flow_configuration)
  : configuration_(std::move(configuration)),
    provenance_repo_(std::move(provenance_repo)),
    flow_file_repo_(std::move(flow_file_repo)),
    flow_configuration_(flow_configuration) {
}

void ResponseNodeLoader::initializeComponentMetrics(core::ProcessGroup* root) {
  {
    std::lock_guard<std::mutex> guard(component_metrics_mutex_);
    component_metrics_.clear();
  }

  if (!root) {
    return;
  }

  std::vector<core::Processor*> processors;
  root->getAllProcessors(processors);
  for (const auto processor : processors) {
    auto node_source = dynamic_cast<ResponseNodeSource*>(processor);
    if (node_source == nullptr) {
      continue;
    }
    // we have a metrics source.
    auto metric = node_source->getResponseNodes();
    std::lock_guard<std::mutex> guard(component_metrics_mutex_);
    component_metrics_[metric->getName()].push_back(metric);
  }
}

std::vector<std::shared_ptr<ResponseNode>> ResponseNodeLoader::getResponseNodes(const std::string& clazz) const {
  std::shared_ptr<core::CoreComponent> ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);
  if (ptr == nullptr) {
    return getComponentMetricsNodes(clazz);
  }
  auto response_node = std::dynamic_pointer_cast<ResponseNode>(ptr);
  if (!response_node) {
    logger_->log_error("Instantiated class '%s' is not a ResponseNode!", clazz);
    return {};
  }
  return {response_node};
}

void ResponseNodeLoader::initializeRepositoryMetrics(const std::shared_ptr<ResponseNode>& response_node) {
  auto repository_metrics = dynamic_cast<RepositoryMetrics*>(response_node.get());
  if (repository_metrics != nullptr) {
    repository_metrics->addRepository(provenance_repo_);
    repository_metrics->addRepository(flow_file_repo_);
  }
}

void ResponseNodeLoader::initializeQueueMetrics(const std::shared_ptr<ResponseNode>& response_node, core::ProcessGroup* root) {
  if (!root) {
    return;
  }

  auto queue_metrics = dynamic_cast<QueueMetrics*>(response_node.get());
  if (queue_metrics != nullptr) {
    std::map<std::string, Connection*> connections;
    root->getConnections(connections);
    for (const auto &con : connections) {
      queue_metrics->updateConnection(con.second);
    }
  }
}

void ResponseNodeLoader::initializeAgentIdentifier(const std::shared_ptr<ResponseNode>& response_node) {
  auto identifier = dynamic_cast<state::response::AgentIdentifier*>(response_node.get());
  if (identifier != nullptr) {
    identifier->setAgentIdentificationProvider(configuration_);
  }
}

void ResponseNodeLoader::initializeAgentMonitor(const std::shared_ptr<ResponseNode>& response_node) {
  auto monitor = dynamic_cast<state::response::AgentMonitor*>(response_node.get());
  if (monitor != nullptr) {
    monitor->addRepository(provenance_repo_);
    monitor->addRepository(flow_file_repo_);
    monitor->setStateMonitor(update_sink_);
  }
}

void ResponseNodeLoader::initializeAgentNode(const std::shared_ptr<ResponseNode>& response_node) {
  auto agent_node = dynamic_cast<state::response::AgentNode*>(response_node.get());
  if (agent_node != nullptr && controller_ != nullptr) {
    agent_node->setUpdatePolicyController(std::static_pointer_cast<controllers::UpdatePolicyControllerService>(controller_->getControllerService(c2::C2Agent::UPDATE_NAME)).get());
  }
  if (agent_node != nullptr) {
    agent_node->setConfigurationReader([this](const std::string& key){
      return configuration_->getRawValue(key);
    });
  }
}

void ResponseNodeLoader::initializeAgentStatus(const std::shared_ptr<ResponseNode>& response_node) {
  auto agent_status = dynamic_cast<state::response::AgentStatus*>(response_node.get());
  if (agent_status != nullptr) {
    agent_status->addRepository(provenance_repo_);
    agent_status->addRepository(flow_file_repo_);
    agent_status->setStateMonitor(update_sink_);
  }
}

void ResponseNodeLoader::initializeConfigurationChecksums(const std::shared_ptr<ResponseNode>& response_node) {
  auto configuration_checksums = dynamic_cast<state::response::ConfigurationChecksums*>(response_node.get());
  if (configuration_checksums) {
    configuration_checksums->addChecksumCalculator(configuration_->getChecksumCalculator());
    if (flow_configuration_) {
      configuration_checksums->addChecksumCalculator(flow_configuration_->getChecksumCalculator());
    }
  }
}

void ResponseNodeLoader::initializeFlowMonitor(const std::shared_ptr<ResponseNode>& response_node, core::ProcessGroup* root) {
  auto flowMonitor = dynamic_cast<state::response::FlowMonitor*>(response_node.get());
  if (flowMonitor == nullptr) {
    return;
  }

  std::map<std::string, Connection*> connections;
  if (root) {
    root->getConnections(connections);
  }

  for (auto &con : connections) {
    flowMonitor->updateConnection(con.second);
  }
  flowMonitor->setStateMonitor(update_sink_);
  if (flow_configuration_) {
    flowMonitor->setFlowVersion(flow_configuration_->getFlowVersion());
  }
}

std::vector<std::shared_ptr<ResponseNode>> ResponseNodeLoader::loadResponseNodes(const std::string& clazz, core::ProcessGroup* root) {
  auto response_nodes = getResponseNodes(clazz);
  if (response_nodes.empty()) {
    logger_->log_error("No metric defined for %s", clazz);
    return {};
  }

  for (const auto& response_node : response_nodes) {
    initializeRepositoryMetrics(response_node);
    initializeQueueMetrics(response_node, root);
    initializeAgentIdentifier(response_node);
    initializeAgentMonitor(response_node);
    initializeAgentNode(response_node);
    initializeAgentStatus(response_node);
    initializeConfigurationChecksums(response_node);
    initializeFlowMonitor(response_node, root);
  }
  return response_nodes;
}

std::vector<std::shared_ptr<ResponseNode>> ResponseNodeLoader::getComponentMetricsNodes(const std::string& metrics_class) const {
  if (!metrics_class.empty()) {
    std::lock_guard<std::mutex> lock(component_metrics_mutex_);
    const auto citer = component_metrics_.find(metrics_class);
    if (citer != component_metrics_.end()) {
      return citer->second;
    }
  }
  return {};
}

void ResponseNodeLoader::setControllerServiceProvider(core::controller::ControllerServiceProvider* controller) {
  controller_ = controller;
}

void ResponseNodeLoader::setStateMonitor(state::StateMonitor* update_sink) {
  update_sink_ = update_sink;
}

}  // namespace org::apache::nifi::minifi::state::response

