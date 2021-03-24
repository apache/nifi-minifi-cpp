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
  MemoryConsumptionCounter() {
  }

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const override {
    rapidjson::Value& group_node = acquireNode(std::string("Memory"), body, alloc);

    rapidjson::Value total_pysical_memory;
    total_pysical_memory.SetInt64(utils::OsUtils::getSystemTotalPhysicalMemory());
    group_node.AddMember(rapidjson::Value("Total Physical Memory", alloc), total_pysical_memory, alloc);

    rapidjson::Value available_physical_memory;
    available_physical_memory.SetInt64(utils::OsUtils::getSystemTotalPhysicalMemory() - utils::OsUtils::getSystemPhysicalMemoryUsage());
    group_node.AddMember(rapidjson::Value("Available Physical Memory", alloc), available_physical_memory, alloc);

    rapidjson::Value total_paging_file_size;
    total_paging_file_size.SetInt64(utils::OsUtils::getTotalPagingFileSize());
    group_node.AddMember(rapidjson::Value("Total paging file size", alloc), total_paging_file_size, alloc);
  }
};
}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
