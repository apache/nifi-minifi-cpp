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
#include "core/state/nodes/ResponseNodeManager.h"

#include <vector>

#include "core/Processor.h"
#include "core/ClassLoader.h"
#include "core/state/nodes/RepositoryMetrics.h"
#include "core/state/nodes/QueueMetrics.h"
#include "core/state/nodes/AgentInformation.h"
#include "core/state/nodes/ConfigurationChecksums.h"
#include "c2/C2Agent.h"

namespace org::apache::nifi::minifi::state::response {

ResponseNodeManager::ResponseNodeManager(std::shared_ptr<Configure> configuration, std::shared_ptr<core::Repository> provenance_repo,
    std::shared_ptr<core::Repository> flow_file_repo, core::FlowConfiguration* flow_configuration)
  : configuration_(configuration),
    provenance_repo_(std::move(provenance_repo)),
    flow_file_repo_(std::move(flow_file_repo)),
    flow_configuration_(flow_configuration) {
}

void ResponseNodeManager::initializeComponentMetrics(core::ProcessGroup* root) {
  {
    std::lock_guard<std::mutex> guard(metrics_mutex_);
    component_metrics_.clear();
  }

  if (root == nullptr) {
    return;
  }
  std::vector<core::Processor*> processors;
  root->getAllProcessors(processors);
  for (const auto processor : processors) {
    auto rep = dynamic_cast<ResponseNodeSource*>(processor);
    if (rep == nullptr) {
      continue;
    }
    // we have a metrics source.
    std::vector<std::shared_ptr<ResponseNode>> metric_vector;
    rep->getResponseNodes(metric_vector);
    std::lock_guard<std::mutex> guard(metrics_mutex_);
    for (auto& metric : metric_vector) {
      component_metrics_[metric->getName()] = metric;
    }
  }
}

std::shared_ptr<ResponseNode> ResponseNodeManager::loadResponseNode(const std::string& clazz, core::ProcessGroup* root) {
  std::shared_ptr<core::CoreComponent> ptr = core::ClassLoader::getDefaultClassLoader().instantiate(clazz, clazz);
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
      return nullptr;
    }
  }
  auto response_node = std::dynamic_pointer_cast<ResponseNode>(ptr);
  auto repository_metrics = dynamic_cast<RepositoryMetrics*>(response_node.get());
  if (repository_metrics != nullptr) {
    repository_metrics->addRepository(provenance_repo_);
    repository_metrics->addRepository(flow_file_repo_);
  }

  auto queue_metrics = dynamic_cast<QueueMetrics*>(response_node.get());
  if (queue_metrics != nullptr) {
    std::map<std::string, Connection*> connections;
    if (root != nullptr) {
      root->getConnections(connections);
    }
    for (auto &con : connections) {
      queue_metrics->updateConnection(con.second);
    }
    std::lock_guard<std::mutex> guard{metrics_mutex_};
    connection_monitors_.insert(queue_metrics);
  }

  auto identifier = dynamic_cast<state::response::AgentIdentifier*>(response_node.get());
  if (identifier != nullptr) {
    identifier->setAgentIdentificationProvider(configuration_);
  }
  auto monitor = dynamic_cast<state::response::AgentMonitor*>(response_node.get());
  if (monitor != nullptr) {
    monitor->addRepository(provenance_repo_);
    monitor->addRepository(flow_file_repo_);
    monitor->setStateMonitor(update_sink_);
  }
  auto agent_node = dynamic_cast<state::response::AgentNode*>(response_node.get());
  if (agent_node != nullptr && controller_ != nullptr) {
    agent_node->setUpdatePolicyController(std::static_pointer_cast<controllers::UpdatePolicyControllerService>(controller_->getControllerService(c2::C2Agent::UPDATE_NAME)).get());
  }
  if (agent_node != nullptr) {
    agent_node->setConfigurationReader([this](const std::string& key){
      return configuration_->getRawValue(key);
    });
  }
  auto configuration_checksums = dynamic_cast<state::response::ConfigurationChecksums*>(response_node.get());
  if (configuration_checksums) {
    configuration_checksums->addChecksumCalculator(configuration_->getChecksumCalculator());
    if (flow_configuration_) {
      configuration_checksums->addChecksumCalculator(flow_configuration_->getChecksumCalculator());
    }
  }
  auto flowMonitor = dynamic_cast<state::response::FlowMonitor*>(response_node.get());
  if (flowMonitor != nullptr) {
    std::map<std::string, Connection*> connections;
    if (root != nullptr) {
      root->getConnections(connections);
    }

    for (auto &con : connections) {
      flowMonitor->updateConnection(con.second);
    }
    flowMonitor->setStateMonitor(update_sink_);
    if (flow_configuration_) {
      flowMonitor->setFlowVersion(flow_configuration_->getFlowVersion());
    }
    std::lock_guard<std::mutex> guard{metrics_mutex_};
    connection_monitors_.insert(flowMonitor);
  }

  return response_node;
}

std::shared_ptr<state::response::ResponseNode> ResponseNodeManager::getComponentMetricsNode(const std::string& metrics_class) const {
  if (metrics_class.empty()) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    const auto citer = component_metrics_.find(metrics_class);
    if (citer != component_metrics_.end()) {
      return citer->second;
    }
  }
  return nullptr;
}

void ResponseNodeManager::updateResponseNodeConnections(core::ProcessGroup* root) {
  std::map<std::string, Connection*> connections;
  if (root != nullptr) {
    root->getConnections(connections);
  }

  std::lock_guard<std::mutex> lock(metrics_mutex_);
  for (auto& connection_monitor : connection_monitors_) {
    connection_monitor->clearConnections();
    for (const auto &con: connections) {
      connection_monitor->updateConnection(con.second);
    }
  }
}

}  // namespace org::apache::nifi::minifi::state::response

