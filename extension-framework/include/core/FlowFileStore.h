// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <unordered_set>
#include <utility>

#include "minifi-cpp/core/FlowFile.h"
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
