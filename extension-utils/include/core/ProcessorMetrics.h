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
#include "minifi-cpp/core/ProcessorMetrics.h"

namespace org::apache::nifi::minifi::core {

template<typename T>
concept Summable = requires(T x) { x + x; };  // NOLINT(readability/braces)

template<typename T>
concept DividableByInteger = requires(T x, uint32_t divisor) { x / divisor; };  // NOLINT(readability/braces)

class Processor;

class ProcessorMetricsImpl : public state::response::ResponseNodeImpl, public virtual ProcessorMetrics {
 public:
  explicit ProcessorMetricsImpl(const Processor& source_processor);

  [[nodiscard]] std::string getName() const override;

  std::vector<state::response::SerializedResponseNode> serialize() override;
  std::vector<state::PublishedMetric> calculateMetrics() override;
  void increaseRelationshipTransferCount(const std::string& relationship, size_t count = 1) override;
  std::chrono::milliseconds getAverageOnTriggerRuntime() const override;
  std::chrono::milliseconds getLastOnTriggerRuntime() const override;
  void addLastOnTriggerRuntime(std::chrono::milliseconds runtime) override;

  std::chrono::milliseconds getAverageSessionCommitRuntime() const override;
  std::chrono::milliseconds getLastSessionCommitRuntime() const override;
  void addLastSessionCommitRuntime(std::chrono::milliseconds runtime) override;

  std::atomic<size_t>& iterations() override {return iterations_;}
  std::atomic<size_t>& transferred_flow_files() override {return transferred_flow_files_;}
  std::atomic<uint64_t>& transferred_bytes() override {return transferred_bytes_;}

  const std::atomic<size_t>& iterations() const override {return iterations_;}
  const std::atomic<size_t>& transferred_flow_files() const override {return transferred_flow_files_;}
  const std::atomic<uint64_t>& transferred_bytes() const override {return transferred_bytes_;}

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

  std::mutex transferred_relationships_mutex_;
  std::unordered_map<std::string, size_t> transferred_relationships_;
  const Processor& source_processor_;
  Averager<std::chrono::milliseconds> on_trigger_runtime_averager_;
  Averager<std::chrono::milliseconds> session_commit_runtime_averager_;

 private:
  std::atomic<size_t> iterations_{0};
  std::atomic<size_t> transferred_flow_files_{0};
  std::atomic<uint64_t> transferred_bytes_{0};
};

}  // namespace org::apache::nifi::minifi::core
