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

#include <memory>
#include <vector>
#include <algorithm>
#include <utility>

#include "core/FlowFile.h"
#include "MinMaxHeap.h"
#include "minifi-cpp/SwapManager.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::test::utils {
struct FlowFileQueueTestAccessor;
}  // namespace org::apache::nifi::minifi::test::utils

namespace org::apache::nifi::minifi::utils {

class FlowFileQueue {
  friend struct test::utils::FlowFileQueueTestAccessor;
  using TimePoint = std::chrono::steady_clock::time_point;

 public:
  using value_type = std::shared_ptr<core::FlowFile>;

  explicit FlowFileQueue(std::shared_ptr<SwapManager> swap_manager = {});

  value_type pop();
  std::optional<value_type> tryPop();
  std::optional<value_type> tryPop(std::chrono::milliseconds timeout);
  void push(value_type element);
  bool isWorkAvailable() const;
  bool empty() const;
  size_t size() const;
  void setMinSize(size_t min_size);
  void setTargetSize(size_t target_size);
  void setMaxSize(size_t max_size);
  void clear();

 private:
  std::optional<value_type> tryPopImpl(std::optional<std::chrono::milliseconds> timeout);

  void initiateLoadIfNeeded();

  struct LoadTask {
    TimePoint min;
    TimePoint max;
    std::future<std::vector<std::shared_ptr<core::FlowFile>>> items;
    size_t count;
    // flow files that have been pushed into the queue while a
    // load was pending
    std::vector<value_type> intermediate_items;

    LoadTask(TimePoint min, TimePoint max, std::future<std::vector<std::shared_ptr<core::FlowFile>>> items, size_t count)
      : min(min), max(max), items(std::move(items)), count(count) {}

    size_t size() const {
      return count + intermediate_items.size();
    }
  };

  bool processLoadTaskWait(std::optional<std::chrono::milliseconds> timeout);

  struct FlowFilePenaltyExpirationComparator {
    bool operator()(const value_type& left, const value_type& right) const;
  };

  struct SwappedFlowFileComparator {
    bool operator()(const SwappedFlowFile& left, const SwappedFlowFile& right) const;
  };

  size_t shouldSwapOutCount() const;

  size_t shouldSwapInCount() const;

  std::shared_ptr<SwapManager> swap_manager_;
  // a load is initiated if the queue_ shrinks below this threshold
  std::atomic<size_t> min_size_{0};
  // a given operation (load/store) will try to approach this size
  std::atomic<size_t> target_size_{0};
  // a store is initiated if the queue_ grows beyond this threshold
  std::atomic<size_t> max_size_{0};

  MinMaxHeap<SwappedFlowFile, SwappedFlowFileComparator> swapped_flow_files_;
  // the pending swap-in operation (if any)
  std::optional<LoadTask> load_task_;
  MinMaxHeap<value_type, FlowFilePenaltyExpirationComparator> queue_;

  std::shared_ptr<timeutils::SteadyClock> clock_{timeutils::getClock()};

  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils
