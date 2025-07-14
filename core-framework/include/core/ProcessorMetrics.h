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

#include "core/state/nodes/ResponseNode.h"
#include "minifi-cpp/core/ProcessorMetrics.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::core {

template<typename T>
concept Summable = requires(T x) { x + x; };  // NOLINT(readability/braces)

template<typename T>
concept DividableByInteger = requires(T x, uint32_t divisor) { x / divisor; };  // NOLINT(readability/braces)

class ProcessorImpl;

class ProcessorMetricsImpl : public state::response::ResponseNodeImpl, public virtual ProcessorMetrics {
 public:
  class ProcessorInfoProvider {
   public:
    virtual std::string getProcessorType() const = 0;
    virtual std::string getName() const = 0;
    virtual utils::SmallString<36> getUUIDStr() const = 0;
    virtual ~ProcessorInfoProvider() = default;
  };
  explicit ProcessorMetricsImpl(const ProcessorImpl& source_processor);
  explicit ProcessorMetricsImpl(std::unique_ptr<ProcessorInfoProvider> source_processor_info_provider);

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
  std::optional<size_t> getTransferredFlowFilesToRelationshipCount(const std::string& relationship) const override;

  std::atomic<size_t>& invocations() override {return invocations_;}
  const std::atomic<size_t>& invocations() const override {return invocations_;}
  std::atomic<size_t>& incomingFlowFiles() override {return incoming_flow_files_;}
  const std::atomic<size_t>& incomingFlowFiles() const override {return incoming_flow_files_;}
  std::atomic<size_t>& transferredFlowFiles() override {return transferred_flow_files_;}
  const std::atomic<size_t>& transferredFlowFiles() const override {return transferred_flow_files_;}
  std::atomic<uint64_t>& incomingBytes() override {return incoming_bytes_;}
  const std::atomic<uint64_t>& incomingBytes() const override {return incoming_bytes_;}
  std::atomic<uint64_t>& transferredBytes() override {return transferred_bytes_;}
  const std::atomic<uint64_t>& transferredBytes() const override {return transferred_bytes_;}
  std::atomic<uint64_t>& bytesRead() override {return bytes_read_;}
  const std::atomic<uint64_t>& bytesRead() const override {return bytes_read_;}
  std::atomic<uint64_t>& bytesWritten() override {return bytes_written_;}
  const std::atomic<uint64_t>& bytesWritten() const override {return bytes_written_;}
  std::atomic<uint64_t>& processingNanos() override {return processing_nanos_;}
  const std::atomic<uint64_t>& processingNanos() const override {return processing_nanos_;}

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
  std::unique_ptr<ProcessorInfoProvider> source_processor_;
  Averager<std::chrono::milliseconds> on_trigger_runtime_averager_;
  Averager<std::chrono::milliseconds> session_commit_runtime_averager_;

 private:
  std::atomic<size_t> invocations_{0};
  std::atomic<size_t> incoming_flow_files_{0};
  std::atomic<size_t> transferred_flow_files_{0};
  std::atomic<uint64_t> incoming_bytes_{0};
  std::atomic<uint64_t> transferred_bytes_{0};
  std::atomic<uint64_t> bytes_read_{0};
  std::atomic<uint64_t> bytes_written_{0};
  std::atomic<uint64_t> processing_nanos_{0};
};

}  // namespace org::apache::nifi::minifi::core
