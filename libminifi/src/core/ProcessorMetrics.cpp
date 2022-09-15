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
#include "core/ProcessorMetrics.h"

#include "core/Processor.h"
#include "utils/gsl.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

ProcessorMetrics::ProcessorMetrics(const Processor& source_processor)
    : source_processor_(source_processor),
      on_trigger_runtime_averager_(STORED_ON_TRIGGER_RUNTIME_COUNT) {
}

std::string ProcessorMetrics::getName() const {
  return source_processor_.getProcessorType() + "Metrics";
}

std::unordered_map<std::string, std::string> ProcessorMetrics::getCommonLabels() const {
  return {{"metric_class", getName()}, {"processor_name", source_processor_.getName()}, {"processor_uuid", source_processor_.getUUIDStr()}};
}

std::vector<state::response::SerializedResponseNode> ProcessorMetrics::serialize() {
  std::vector<state::response::SerializedResponseNode> resp;

  state::response::SerializedResponseNode root_node;
  root_node.name = source_processor_.getUUIDStr();

  state::response::SerializedResponseNode iter;
  iter.name = "OnTriggerInvocations";
  iter.value = static_cast<uint32_t>(iterations.load());

  root_node.children.push_back(iter);

  state::response::SerializedResponseNode average_ontrigger_runtime_node;
  average_ontrigger_runtime_node.name = "AverageOnTriggerRunTime";
  average_ontrigger_runtime_node.value = static_cast<uint64_t>(getAverageOnTriggerRuntime().count());

  root_node.children.push_back(average_ontrigger_runtime_node);

  state::response::SerializedResponseNode last_ontrigger_runtime_node;
  last_ontrigger_runtime_node.name = "LastOnTriggerRunTime";
  last_ontrigger_runtime_node.value = static_cast<uint64_t>(getLastOnTriggerRuntime().count());

  root_node.children.push_back(last_ontrigger_runtime_node);

  state::response::SerializedResponseNode transferred_flow_files_node;
  transferred_flow_files_node.name = "TransferredFlowFiles";
  transferred_flow_files_node.value = static_cast<uint32_t>(transferred_flow_files.load());

  root_node.children.push_back(transferred_flow_files_node);

  for (const auto& [relationship, count] : transferred_relationships_) {
    state::response::SerializedResponseNode transferred_to_relationship_node;
    gsl_Expects(relationship.size() > 0);
    transferred_to_relationship_node.name = std::string("TransferredTo").append(1, toupper(relationship[0])).append(relationship.substr(1));
    transferred_to_relationship_node.value = static_cast<uint32_t>(count);

    root_node.children.push_back(transferred_to_relationship_node);
  }

  state::response::SerializedResponseNode transferred_bytes_node;
  transferred_bytes_node.name = "TransferredBytes";
  transferred_bytes_node.value = transferred_bytes.load();

  root_node.children.push_back(transferred_bytes_node);

  resp.push_back(root_node);

  return resp;
}

std::vector<state::PublishedMetric> ProcessorMetrics::calculateMetrics() {
  std::vector<state::PublishedMetric> metrics = {
    {"onTrigger_invocations", static_cast<double>(iterations.load()), getCommonLabels()},
    {"average_onTrigger_runtime_milliseconds", static_cast<double>(getAverageOnTriggerRuntime().count()), getCommonLabels()},
    {"last_onTrigger_runtime_milliseconds", static_cast<double>(getLastOnTriggerRuntime().count()), getCommonLabels()},
    {"transferred_flow_files", static_cast<double>(transferred_flow_files.load()), getCommonLabels()},
    {"transferred_bytes", static_cast<double>(transferred_bytes.load()), getCommonLabels()}
  };

  {
    std::lock_guard<std::mutex> lock(transferred_relationships_mutex_);
    for (const auto& [relationship, count] : transferred_relationships_) {
      metrics.push_back({"transferred_to_" + relationship, static_cast<double>(count),
        {{"metric_class", getName()}, {"processor_name", source_processor_.getName()}, {"processor_uuid", source_processor_.getUUIDStr()}}});
    }
  }

  return metrics;
}

void ProcessorMetrics::incrementRelationshipTransferCount(const std::string& relationship) {
  std::lock_guard<std::mutex> lock(transferred_relationships_mutex_);
  ++transferred_relationships_[relationship];
}

std::chrono::milliseconds ProcessorMetrics::getAverageOnTriggerRuntime() const {
  return on_trigger_runtime_averager_.getAverage();
}

void ProcessorMetrics::addLastOnTriggerRuntime(std::chrono::milliseconds runtime) {
  on_trigger_runtime_averager_.addValue(runtime);
}

std::chrono::milliseconds ProcessorMetrics::getLastOnTriggerRuntime() const {
  return on_trigger_runtime_averager_.getLastValue();
}

}  // namespace org::apache::nifi::minifi::core
