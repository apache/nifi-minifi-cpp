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

#include "core/state/nodes/FlowInformation.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::state::response {

std::vector<SerializedResponseNode> FlowVersion::serialize() {
  std::lock_guard<std::mutex> lock(guard);
  return {
    {.name = "registryUrl", .value = identifier->getRegistryUrl()},
    {.name = "bucketId", .value = identifier->getBucketId()},
    {.name = "flowId", .value = identifier->getFlowId()}
  };
}

std::vector<SerializedResponseNode> FlowInformation::serialize() {
  std::vector<SerializedResponseNode> serialized  = {
    {.name = "flowId", .value = flow_version_->getFlowId()}
  };

  SerializedResponseNode uri;
  uri.name = "versionedFlowSnapshotURI";
  for (auto &entry : flow_version_->serialize()) {
    uri.children.push_back(entry);
  }
  serialized.push_back(uri);

  const auto& connections = connection_store_.getConnections();
  if (!connections.empty()) {
    SerializedResponseNode queues{.name = "queues", .collapsible = false};

    for (const auto& queue : connections) {
      queues.children.push_back({
        .name = queue.second->getName(),
        .collapsible = false,
        .children = {
          {.name = "size", .value = queue.second->getQueueSize()},
          {.name = "sizeMax", .value = queue.second->getBackpressureThresholdCount()},
          {.name = "dataSize", .value = queue.second->getQueueDataSize()},
          {.name = "dataSizeMax", .value = queue.second->getBackpressureThresholdDataSize()},
          {.name = "uuid", .value = std::string{queue.second->getUUIDStr()}}
        }
      });
    }
    serialized.push_back(queues);
  }

  if (nullptr != monitor_) {
    SerializedResponseNode componentsNode{.name = "components", .collapsible = false};
    monitor_->executeOnAllComponents([&componentsNode](StateController& component){
      componentsNode.children.push_back({
        .name = component.getComponentName(),
        .collapsible = false,
        .children = {
          {.name = "running", .value = component.isRunning()},
          {.name = "uuid", .value = std::string{component.getComponentUUID().to_string()}}
        }
      });
    });
    serialized.push_back(componentsNode);
  }

  return serialized;
}

std::vector<PublishedMetric> FlowInformation::calculateMetrics() {
  std::vector<PublishedMetric> metrics = connection_store_.calculateConnectionMetrics("FlowInformation");

  if (nullptr != monitor_) {
    monitor_->executeOnAllComponents([&metrics](StateController& component){
      metrics.push_back({"is_running", (component.isRunning() ? 1.0 : 0.0),
        {{"component_uuid", component.getComponentUUID().to_string()}, {"component_name", component.getComponentName()}, {"metric_class", "FlowInformation"}}});
    });
  }
  return metrics;
}

REGISTER_RESOURCE(FlowInformation, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
