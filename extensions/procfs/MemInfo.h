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

#include <stdint.h>
#include <fstream>
#include <optional>

namespace org::apache::nifi::minifi::extensions::procfs {

class MemInfo {
 private:
  MemInfo() = default;

 public:
  static std::optional<MemInfo> parseMemInfoFile(std::ifstream& mem_info_file);

  [[nodiscard]] uint64_t getTotalMemory() const noexcept { return memory_total_; }
  [[nodiscard]] uint64_t getFreeMemory() const noexcept { return memory_free_; }
  [[nodiscard]] uint64_t getAvailableMemory() const noexcept { return memory_available_; }
  [[nodiscard]] uint64_t getTotalSwap() const noexcept { return swap_total_; }
  [[nodiscard]] uint64_t getFreeSwap() const noexcept { return swap_free_; }

 private:
  uint64_t memory_total_;
  uint64_t memory_free_;
  uint64_t memory_available_;
  uint64_t swap_total_;
  uint64_t swap_free_;
};

}  // namespace org::apache::nifi::minifi::extensions::procfs
