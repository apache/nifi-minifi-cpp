/**
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
#include "c2/ControllerSocketMetricsPublisher.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/Resource.h"
#include "c2/C2Agent.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace org::apache::nifi::minifi::c2 {

std::unordered_map<std::string, ControllerSocketReporter::QueueSize> ControllerSocketMetricsPublisher::getQueueSizes() {
  std::lock_guard<std::mutex> guard(queue_metrics_node_mutex_);
  std::unordered_map<std::string, QueueSize> sizes;
  if (!queue_metrics_node_) {
    return sizes;
  }
  for (const auto& metric : queue_metrics_node_->calculateMetrics()) {
    gsl_Expects(metric.labels.contains("connection_name"));
    if (metric.name == "queue_size") {
      sizes[metric.labels.at("connection_name")].queue_size = static_cast<uint32_t>(metric.value);
    } else if (metric.name == "queue_size_max") {
      sizes[metric.labels.at("connection_name")].queue_size_max = static_cast<uint32_t>(metric.value);
    }
  }
  return sizes;
}

std::unordered_set<std::string> ControllerSocketMetricsPublisher::getFullConnections() {
  std::unordered_set<std::string> full_connections;
  auto queues = getQueueSizes();
  for (const auto& [name, queue] : queues) {
    if (queue.queue_size_max == 0) {
      continue;
    }
    if (queue.queue_size >= queue.queue_size_max) {
      full_connections.insert(name);
    }
  }
  return full_connections;
}

std::unordered_set<std::string> ControllerSocketMetricsPublisher::getConnections() {
  std::lock_guard<std::mutex> guard(queue_metrics_node_mutex_);
  std::unordered_set<std::string> connections;
  if (!queue_metrics_node_) {
    return connections;
  }
  for (const auto& metric : queue_metrics_node_->calculateMetrics()) {
    gsl_Expects(metric.labels.contains("connection_name"));
    connections.insert(metric.labels.at("connection_name"));
  }
  return connections;
}

std::string ControllerSocketMetricsPublisher::getAgentManifest() {
  C2Payload agent_manifest(Operation::describe);
  agent_manifest.setLabel("agentManifest");
  auto manifest = response_node_loader_->getAgentManifest();
  C2Agent::serializeMetrics(agent_manifest, manifest.name, manifest.serialized_nodes);
  return heartbeat_json_serializer_.serializeJsonRootPayload(agent_manifest);
}

void ControllerSocketMetricsPublisher::clearMetricNodes() {
  std::lock_guard<std::mutex> guard(queue_metrics_node_mutex_);
  queue_metrics_node_.reset();
}

void ControllerSocketMetricsPublisher::loadMetricNodes() {
  std::lock_guard<std::mutex> guard(queue_metrics_node_mutex_);
  gsl_Expects(response_node_loader_);
  auto nodes = response_node_loader_->loadResponseNodes("QueueMetrics");
  if (!nodes.empty()) {
    queue_metrics_node_ = nodes[0];
  }
}

void ControllerSocketMetricsPublisher::setRoot(core::ProcessGroup* root) {
  flow_status_builder_.setRoot(root);
}

void ControllerSocketMetricsPublisher::setFlowStatusDependencies(core::BulletinStore* bulletin_store, const std::filesystem::path& flowfile_repo_dir, const std::filesystem::path& content_repo_dir) {
  flow_status_builder_.setBulletinStore(bulletin_store);
  flow_status_builder_.setRepositoryPaths(flowfile_repo_dir, content_repo_dir);
}

std::string ControllerSocketMetricsPublisher::getFlowStatus(const std::vector<FlowStatusRequest>& requests) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  auto status = flow_status_builder_.buildFlowStatus(requests);
  status.Accept(writer);
  return buffer.GetString();
}

REGISTER_RESOURCE(ControllerSocketMetricsPublisher, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::c2
