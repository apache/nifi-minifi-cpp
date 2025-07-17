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
#include <unordered_set>
#include <utility>

#include "FlowFile.h"
namespace org::apache::nifi::minifi::core {

class FlowFileStore {
 public:
  std::unordered_set<std::shared_ptr<FlowFile>> getNewFlowFiles() {
    bool hasNewFlowFiles = true;
    if (!has_new_flow_file_.compare_exchange_strong(hasNewFlowFiles, false, std::memory_order_acquire, std::memory_order_relaxed)) {
      return {};
    }
    std::lock_guard<std::mutex> guard(flow_file_mutex_);
    return std::move(incoming_files_);
  }

  void put(const std::shared_ptr<FlowFile>& flowFile)  {
    {
      std::lock_guard<std::mutex> guard(flow_file_mutex_);
      incoming_files_.emplace(flowFile);
    }
    has_new_flow_file_.store(true, std::memory_order_release);
  }
 private:
  std::atomic_bool has_new_flow_file_{false};
  std::mutex flow_file_mutex_;
  std::unordered_set<std::shared_ptr<FlowFile>> incoming_files_;
};

}  // namespace org::apache::nifi::minifi::core
