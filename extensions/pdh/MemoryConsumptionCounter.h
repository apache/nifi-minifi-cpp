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

#include "PerformanceDataCounter.h"
#include <string>
#include "utils/OsUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class MemoryConsumptionCounter : public PerformanceDataCounter {
 public:
  MemoryConsumptionCounter() : PerformanceDataCounter(), total_physical_memory_(-1), available_physical_memory_(-1), total_paging_file_size_(-1) {
  }

  bool dataIsValid() {
    return total_physical_memory_ > 0 && available_physical_memory_ > 0 && total_paging_file_size_ > 0;
  }

  bool collectData() override {
    total_physical_memory_ = utils::OsUtils::getSystemTotalPhysicalMemory();
    available_physical_memory_ = utils::OsUtils::getSystemTotalPhysicalMemory() - utils::OsUtils::getSystemPhysicalMemoryUsage();
    total_paging_file_size_ = utils::OsUtils::getTotalPagingFileSize();
    return dataIsValid();
  }

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const override {
    rapidjson::Value& group_node = acquireNode(std::string("Memory"), body, alloc);

    rapidjson::Value total_physical_memory_value;
    total_physical_memory_value.SetInt64(total_physical_memory_);
    group_node.AddMember(rapidjson::Value("Total Physical Memory", alloc), total_physical_memory_value, alloc);

    rapidjson::Value available_physical_memory_value;
    available_physical_memory_value.SetInt64(available_physical_memory_);
    group_node.AddMember(rapidjson::Value("Available Physical Memory", alloc), available_physical_memory_value, alloc);

    rapidjson::Value total_paging_file_size_value;
    total_paging_file_size_value.SetInt64(total_paging_file_size_);
    group_node.AddMember(rapidjson::Value("Total paging file size", alloc), total_paging_file_size_value, alloc);
  }

  int64_t total_physical_memory_;
  int64_t available_physical_memory_;
  int64_t total_paging_file_size_;
};
}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
