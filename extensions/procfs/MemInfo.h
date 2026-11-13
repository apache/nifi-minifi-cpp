// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
