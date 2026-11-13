// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <unistd.h>

#include <map>
#include <vector>
#include <utility>
#include <string>
#include <optional>
#include <filesystem>
#include <memory>

#include "ProcessStat.h"
#include "MemInfo.h"
#include "CpuStat.h"
#include "NetDev.h"
#include "DiskStat.h"

#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::extensions::procfs {

class ProcFs {
  static constexpr const char* DEFAULT_ROOT_PATH = "/proc";
  static constexpr const char* MEMINFO_FILE = "meminfo";
  static constexpr const char* STAT_FILE = "stat";
  static constexpr const char* NET_DEV_FILE = "net/dev";
  static constexpr const char* DISK_STATS_FILE = "diskstats";
 public:
  explicit ProcFs(std::filesystem::path path = DEFAULT_ROOT_PATH)
      : root_path_(std::move(path)) {
  }

  [[nodiscard]] std::map<pid_t, ProcessStat> getProcessStats() const;
  [[nodiscard]] std::vector<std::pair<std::string, CpuStatData>> getCpuStats() const;
  [[nodiscard]] std::vector<std::pair<std::string, NetDevData>> getNetDevs() const;
  [[nodiscard]] std::vector<std::pair<std::string, DiskStatData>> getDiskStats() const;
  [[nodiscard]] std::optional<MemInfo> getMemInfo() const;

 private:
  std::filesystem::path root_path_;
  uint64_t page_size_ = sysconf(_SC_PAGESIZE);
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ProcFs>::getLogger();
};


}  // namespace org::apache::nifi::minifi::extensions::procfs
