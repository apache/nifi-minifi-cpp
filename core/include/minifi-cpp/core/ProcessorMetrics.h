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

#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "minifi-cpp/core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::core {

class ProcessorMetrics : public virtual state::response::ResponseNode {
 public:
  virtual void increaseRelationshipTransferCount(const std::string& relationship, size_t count = 1) = 0;
  virtual std::chrono::milliseconds getAverageOnTriggerRuntime() const = 0;
  virtual std::chrono::milliseconds getLastOnTriggerRuntime() const = 0;
  virtual void addLastOnTriggerRuntime(std::chrono::milliseconds runtime) = 0;

  virtual std::chrono::milliseconds getAverageSessionCommitRuntime() const = 0;
  virtual std::chrono::milliseconds getLastSessionCommitRuntime() const = 0;
  virtual void addLastSessionCommitRuntime(std::chrono::milliseconds runtime) = 0;

  virtual std::atomic<size_t>& invocations() = 0;
  virtual const std::atomic<size_t>& invocations() const = 0;
  virtual std::atomic<size_t>& incoming_flow_files() = 0;
  virtual const std::atomic<size_t>& incoming_flow_files() const = 0;
  virtual std::atomic<size_t>& transferred_flow_files() = 0;
  virtual const std::atomic<size_t>& transferred_flow_files() const = 0;
  virtual std::atomic<uint64_t>& incoming_bytes() = 0;
  virtual const std::atomic<uint64_t>& incoming_bytes() const = 0;
  virtual std::atomic<uint64_t>& transferred_bytes() = 0;
  virtual const std::atomic<uint64_t>& transferred_bytes() const = 0;
  virtual std::atomic<uint64_t>& bytes_read() = 0;
  virtual const std::atomic<uint64_t>& bytes_read() const = 0;
  virtual std::atomic<uint64_t>& bytes_written() = 0;
  virtual const std::atomic<uint64_t>& bytes_written() const = 0;
  virtual std::atomic<uint64_t>& processing_nanos() = 0;
  virtual const std::atomic<uint64_t>& processing_nanos() const = 0;
};

}  // namespace org::apache::nifi::minifi::core
