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

#include "MemInfo.h"
#include <string>
#include <sstream>

namespace org::apache::nifi::minifi::extensions::procfs {

std::optional<MemInfo> MemInfo::parseMemInfoFile(std::ifstream& mem_info_file) {
  bool kb_memory_total_found = false;
  bool kb_memory_free_found = false;
  bool kb_memory_available_found = false;
  bool kb_swap_total_found = false;
  bool kb_swap_free_found = false;
  std::string line;
  MemInfo mem_info;
  while (std::getline(mem_info_file, line)) {
    std::istringstream iss(line);
    std::string entry_name;
    iss >> entry_name;
    if (entry_name == "MemTotal:") {
      iss >> mem_info.memory_total_;
      mem_info.memory_total_ *= 1024;
      kb_memory_total_found = true;
    } else if (entry_name == "MemFree:") {
      iss >> mem_info.memory_free_;
      mem_info.memory_free_ *= 1024;
      kb_memory_free_found = true;
    } else if (entry_name == "MemAvailable:") {
      iss >> mem_info.memory_available_;
      mem_info.memory_available_ *= 1024;
      kb_memory_available_found = true;
    } else if (entry_name == "SwapTotal:") {
      iss >> mem_info.swap_total_;
      mem_info.swap_total_ *= 1024;
      kb_swap_total_found = true;
    } else if (entry_name == "SwapFree:") {
      iss >> mem_info.swap_free_;
      mem_info.swap_free_ *= 1024;
      kb_swap_free_found = true;
    }
    if (iss.fail())
      return std::nullopt;
    if (kb_memory_total_found && kb_memory_available_found && kb_memory_free_found && kb_swap_total_found && kb_swap_free_found)
      return mem_info;
  }
  return std::nullopt;
}
}  // namespace org::apache::nifi::minifi::extensions::procfs
