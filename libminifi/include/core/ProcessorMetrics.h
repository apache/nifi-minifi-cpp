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
#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <vector>

#include "core/state/nodes/MetricsBase.h"
#include "core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::core {

template<typename T>
concept Summable = requires(T x) { x + x; };  // NOLINT(readability/braces)

template<typename T>
concept DividableByInteger = requires(T x, uint32_t divisor) { x / divisor; };  // NOLINT(readability/braces)

class Processor;

class ProcessorMetrics : public state::response::ResponseNode {
 public:
  explicit ProcessorMetrics(const Processor& source_processor);

  [[nodiscard]] std::string getName() const override;

  std::vector<state::response::SerializedResponseNode> serialize() override;
  std::vector<state::PublishedMetric> calculateMetrics() override;
  void increaseRelationshipTransferCount(const std::string& relationship, size_t count = 1);
  std::chrono::milliseconds getAverageOnTriggerRuntime() const;
  std::chrono::milliseconds getLastOnTriggerRuntime() const;
  void addLastOnTriggerRuntime(std::chrono::milliseconds runtime);

  std::chrono::milliseconds getAverageSessionCommitRuntime() const;
  std::chrono::milliseconds getLastSessionCommitRuntime() const;
  void addLastSessionCommitRuntime(std::chrono::milliseconds runtime);
  std::optional<size_t> getTransferredFlowFilesToRelationshipCount(const std::string& relationship) const;

  std::atomic<size_t> invocations{0};
  std::atomic<size_t> incoming_flow_files{0};
  std::atomic<size_t> transferred_flow_files{0};
  std::atomic<uint64_t> incoming_bytes{0};
  std::atomic<uint64_t> transferred_bytes{0};
  std::atomic<uint64_t> bytes_read{0};
  std::atomic<uint64_t> bytes_written{0};
  std::atomic<uint64_t> processing_nanos{0};

 protected:
  template<typename ValueType>
  requires Summable<ValueType> && DividableByInteger<ValueType>
  class Averager {
   public:
    explicit Averager(uint32_t sample_size) : SAMPLE_SIZE_(sample_size), next_average_index_(SAMPLE_SIZE_) {
      values_.reserve(SAMPLE_SIZE_);
    }

    ValueType getAverage() const;
    ValueType getLastValue() const;
    void addValue(ValueType runtime);

   private:
    const uint32_t SAMPLE_SIZE_;
    mutable std::mutex average_value_mutex_;
    uint32_t next_average_index_;
    std::vector<ValueType> values_;
  };

  [[nodiscard]] std::unordered_map<std::string, std::string> getCommonLabels() const;
  static constexpr uint8_t STORED_ON_TRIGGER_RUNTIME_COUNT = 10;

  mutable std::mutex transferred_relationships_mutex_;
  std::unordered_map<std::string, size_t> transferred_relationships_;
  const Processor& source_processor_;
  Averager<std::chrono::milliseconds> on_trigger_runtime_averager_;
  Averager<std::chrono::milliseconds> session_commit_runtime_averager_;
};

}  // namespace org::apache::nifi::minifi::core
