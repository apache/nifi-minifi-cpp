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
#include "core/state/Value.h"
#include "minifi-cpp/utils/gsl.h"
#include "range/v3/numeric/accumulate.hpp"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core {

ProcessorMetrics::ProcessorMetrics(const Processor& source_processor)
    : source_processor_(source_processor),
      on_trigger_runtime_averager_(STORED_ON_TRIGGER_RUNTIME_COUNT),
      session_commit_runtime_averager_(STORED_ON_TRIGGER_RUNTIME_COUNT) {
}

std::string ProcessorMetrics::getName() const {
  return source_processor_.getProcessorType() + "Metrics";
}

std::unordered_map<std::string, std::string> ProcessorMetrics::getCommonLabels() const {
  return {{"metric_class", getName()}, {"processor_name", source_processor_.getName()}, {"processor_uuid", source_processor_.getUUIDStr()}};
}

std::vector<state::response::SerializedResponseNode> ProcessorMetrics::serialize() {
  std::vector<state::response::SerializedResponseNode> resp;

  state::response::SerializedResponseNode root_node {
    .name = source_processor_->getUUIDStr(),
    .children = {
      {.name = "OnTriggerInvocations", .value = static_cast<uint32_t>(invocations())},
      {.name = "AverageOnTriggerRunTime", .value = static_cast<uint64_t>(getAverageOnTriggerRuntime().count())},
      {.name = "LastOnTriggerRunTime", .value = static_cast<uint64_t>(getLastOnTriggerRuntime().count())},
      {.name = "AverageSessionCommitRunTime", .value = static_cast<uint64_t>(getAverageSessionCommitRuntime().count())},
      {.name = "LastSessionCommitRunTime", .value = static_cast<uint64_t>(getLastSessionCommitRuntime().count())},
      {.name = "TransferredFlowFiles", .value = static_cast<uint32_t>(transferredFlowFiles())},
      {.name = "TransferredBytes", .value = static_cast<uint64_t>(transferredBytes())},
      {.name = "IncomingFlowFiles", .value = static_cast<uint32_t>(incomingFlowFiles())},
      {.name = "IncomingBytes", .value = static_cast<uint64_t>(incomingBytes())},
      {.name = "BytesRead", .value = static_cast<uint64_t>(bytesRead())},
      {.name = "BytesWritten", .value = static_cast<uint64_t>(bytesWritten())},
      {.name = "ProcessingNanos", .value = static_cast<uint64_t>(processingNanos())}
    }
  };

  {
    std::lock_guard<std::mutex> lock(transferred_relationships_mutex_);
    for (const auto& [relationship, count] : transferred_relationships_) {
      gsl_Expects(!relationship.empty());
      state::response::SerializedResponseNode transferred_to_relationship_node;
      transferred_to_relationship_node.name = std::string("TransferredTo").append(1, gsl::narrow<char>(toupper(relationship[0]))).append(relationship.substr(1));
      transferred_to_relationship_node.value = static_cast<uint32_t>(count);

      root_node.children.push_back(transferred_to_relationship_node);
    }
  }

  resp.push_back(root_node);

  if (auto metrics_extension = source_processor_.getMetricsExtension()) {
    for (auto&& extension_child : metrics_extension->serialize()) {
      resp[0].children.push_back(std::move(extension_child));
    }
  }

  return resp;
}

std::vector<state::PublishedMetric> ProcessorMetrics::calculateMetrics() {
  std::vector<state::PublishedMetric> metrics = {
    {"onTrigger_invocations", static_cast<double>(invocations()), getCommonLabels()},
    {"average_onTrigger_runtime_milliseconds", static_cast<double>(getAverageOnTriggerRuntime().count()), getCommonLabels()},
    {"last_onTrigger_runtime_milliseconds", static_cast<double>(getLastOnTriggerRuntime().count()), getCommonLabels()},
    {"average_session_commit_runtime_milliseconds", static_cast<double>(getAverageSessionCommitRuntime().count()), getCommonLabels()},
    {"last_session_commit_runtime_milliseconds", static_cast<double>(getLastSessionCommitRuntime().count()), getCommonLabels()},
    {"transferred_flow_files", static_cast<double>(transferredFlowFiles()), getCommonLabels()},
    {"transferred_bytes", static_cast<double>(transferredBytes()), getCommonLabels()},
    {"incoming_flow_files", static_cast<double>(incomingFlowFiles()), getCommonLabels()},
    {"incoming_bytes", static_cast<double>(incomingBytes()), getCommonLabels()},
    {"bytes_read", static_cast<double>(bytesRead()), getCommonLabels()},
    {"bytes_written", static_cast<double>(bytesWritten()), getCommonLabels()},
    {"processing_nanos", static_cast<double>(processingNanos()), getCommonLabels()}
  };

  {
    std::lock_guard<std::mutex> lock(transferred_relationships_mutex_);
    for (const auto& [relationship, count] : transferred_relationships_) {
      metrics.push_back({"transferred_to_" + relationship, static_cast<double>(count),
        {{"metric_class", getName()}, {"processor_name", source_processor_->getName()}, {"processor_uuid", source_processor_->getUUIDStr()}}});
    }
  }

  const auto common_labels = getCommonLabels();
  if (auto metrics_extension = source_processor_.getMetricsExtension()) {
    for (auto&& extension_node : metrics_extension->calculateMetrics()) {
      extension_node.labels.insert(common_labels.begin(), common_labels.end());
      metrics.push_back(std::move(extension_node));
    }
  }

  return metrics;
}

void ProcessorMetrics::increaseRelationshipTransferCount(const std::string& relationship, size_t count) {
  std::lock_guard<std::mutex> lock(transferred_relationships_mutex_);
  transferred_relationships_[relationship] += count;
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

std::chrono::milliseconds ProcessorMetrics::getAverageSessionCommitRuntime() const {
  return session_commit_runtime_averager_.getAverage();
}

void ProcessorMetrics::addLastSessionCommitRuntime(std::chrono::milliseconds runtime) {
  session_commit_runtime_averager_.addValue(runtime);
}

std::chrono::milliseconds ProcessorMetrics::getLastSessionCommitRuntime() const {
  return session_commit_runtime_averager_.getLastValue();
}

std::optional<size_t> ProcessorMetrics::getTransferredFlowFilesToRelationshipCount(const std::string& relationship) const {
  std::lock_guard<std::mutex> lock(transferred_relationships_mutex_);
  const auto relationship_it = transferred_relationships_.find(relationship);
  if (relationship_it != std::end(transferred_relationships_)) {
    return relationship_it->second;
  }
  return {};
}

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
ValueType ProcessorMetrics::Averager<ValueType>::getAverage() const {
  std::lock_guard<std::mutex> lock(average_value_mutex_);
  if (values_.empty()) {
    return {};
  }
  return ranges::accumulate(values_, ValueType{}) / values_.size();
}

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
void ProcessorMetrics::Averager<ValueType>::addValue(ValueType runtime) {
  std::lock_guard<std::mutex> lock(average_value_mutex_);
  if (values_.size() < SAMPLE_SIZE_) {
    values_.push_back(runtime);
  } else {
    if (next_average_index_ >= values_.size()) {
      next_average_index_ = 0;
    }
    values_[next_average_index_] = runtime;
    ++next_average_index_;
  }
}

template<typename ValueType>
requires Summable<ValueType> && DividableByInteger<ValueType>
ValueType ProcessorMetrics::Averager<ValueType>::getLastValue() const {
  std::lock_guard<std::mutex> lock(average_value_mutex_);
  if (values_.empty()) {
    return {};
  } else if (values_.size() < SAMPLE_SIZE_) {
    return values_[values_.size() - 1];
  } else {
    return values_[next_average_index_ - 1];
  }
}

}  // namespace org::apache::nifi::minifi::core
