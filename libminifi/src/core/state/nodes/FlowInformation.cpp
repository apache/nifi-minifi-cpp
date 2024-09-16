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
#include "core/state/Value.h"

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

  if (nullptr != monitor_) {
    monitor_->executeOnComponent("FlowController", [&serialized](StateController& component) {
      serialized.push_back({.name = "runStatus", .value = (component.isRunning() ? "RUNNING" : "STOPPED")});
    });
  }

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

  if (!processors_.empty()) {
    SerializedResponseNode processorsStatusesNode{.name = "processorStatuses", .array = true, .collapsible = false};
    for (const auto processor : processors_) {
      if (!processor) {
        continue;
      }

      auto metrics = processor->getMetrics();
      processorsStatusesNode.children.push_back({
        .name = processor->getName(),
        .collapsible = false,
        .children = {
          {.name = "id", .value = std::string{processor->getUUIDStr()}},
          {.name = "groupId", .value = processor->getProcessGroupUUIDStr()},
          {.name = "bytesRead", .value = metrics->bytesRead().load()},
          {.name = "bytesWritten", .value = metrics->bytesWritten().load()},
          {.name = "flowFilesIn", .value = metrics->incomingFlowFiles().load()},
          {.name = "flowFilesOut", .value = metrics->transferredFlowFiles().load()},
          {.name = "bytesIn", .value = metrics->incomingBytes().load()},
          {.name = "bytesOut", .value = metrics->transferredBytes().load()},
          {.name = "invocations", .value = metrics->invocations().load()},
          {.name = "processingNanos", .value = metrics->processingNanos().load()},
          {.name = "activeThreadCount", .value = -1},
          {.name = "terminatedThreadCount", .value = -1},
          {.name = "runStatus", .value = (processor->isRunning() ? "RUNNING" : "STOPPED")}
        }
      });
    }
    serialized.push_back(processorsStatusesNode);
  }

  return serialized;
}

std::vector<PublishedMetric> FlowInformation::calculateMetrics() {
  std::vector<PublishedMetric> metrics = connection_store_.calculateConnectionMetrics("FlowInformation");
  if (nullptr != monitor_) {
    monitor_->executeOnComponent("FlowController", [&metrics](StateController& component) {
      metrics.push_back({"is_running", (component.isRunning() ? 1.0 : 0.0),
        {{"component_uuid", component.getComponentUUID().to_string()}, {"component_name", component.getComponentName()}, {"metric_class", "FlowInformation"}}});
    });
  }

  for (const auto& processor : processors_) {
    if (!processor) {
      continue;
    }
    auto processor_metrics = processor->getMetrics();
    metrics.push_back({"bytes_read", gsl::narrow<double>(processor_metrics->bytesRead().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"bytes_written", gsl::narrow<double>(processor_metrics->bytesWritten().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"flow_files_in", gsl::narrow<double>(processor_metrics->incomingFlowFiles().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"flow_files_out", gsl::narrow<double>(processor_metrics->transferredFlowFiles().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"bytes_in", gsl::narrow<double>(processor_metrics->incomingBytes().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"bytes_out", gsl::narrow<double>(processor_metrics->transferredBytes().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"invocations", gsl::narrow<double>(processor_metrics->invocations().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"processing_nanos", gsl::narrow<double>(processor_metrics->processingNanos().load()),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
    metrics.push_back({"is_running", (processor->isRunning() ? 1.0 : 0.0),
        {{"processor_uuid", processor->getUUIDStr()}, {"processor_name", processor->getName()}, {"metric_class", "FlowInformation"}}});
  }

  return metrics;
}

REGISTER_RESOURCE(FlowInformation, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
