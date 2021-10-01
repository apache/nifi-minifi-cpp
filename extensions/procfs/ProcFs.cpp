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

#include "ProcFs.h"

namespace org::apache::nifi::minifi::procfs {

namespace {
bool is_number(const std::string& s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}
}  // namespace

std::unordered_map<pid_t, ProcessStat> ProcFs::getProcessStats() const {
  std::unordered_map<pid_t, ProcessStat> process_stats;
  for (const auto &entry : std::filesystem::directory_iterator(root_path_)) {
    if (entry.is_directory() && is_number(entry.path().filename())) {
      auto stat_file_path = entry.path() / STAT_FILE;
      std::ifstream stat_file(stat_file_path);
      auto process_stat_data = ProcessStatData::parseProcessStatFile(stat_file);
      if (process_stat_data.has_value())
        process_stats.emplace(process_stat_data->getPid(), ProcessStat(process_stat_data.value(), page_size_));
    }
  }
  return process_stats;
}

std::vector<CpuStat> ProcFs::getCpuStats() const {
  std::vector<CpuStat> cpu_stats;
  auto stat_file_path = root_path_ / STAT_FILE;
  std::ifstream stat_file;
  stat_file.open(stat_file_path);
  std::string line;
  while (std::getline(stat_file, line)) {
    std::istringstream iss(line);
    std::string entry_name;
    iss >> entry_name;
    if (entry_name.find("cpu") != entry_name.npos) {
      auto cpu_stat = CpuStatData::parseCpuStatLine(iss);
      if (cpu_stat.has_value())
        cpu_stats.emplace_back(entry_name, cpu_stat.value());
    }
  }
  return cpu_stats;
}

std::optional<MemInfo> ProcFs::getMemInfo() const {
  auto mem_info_file_path = root_path_ / MEMINFO_FILE;
  std::ifstream mem_info_file(mem_info_file_path);
  return MemInfo::parseMemInfoFile(mem_info_file);
}

std::vector<NetDev> ProcFs::getNetDevs() const {
  std::vector<NetDev> net_devs;
  auto stat_file_path = root_path_ / NET_DEV_FILE;
  std::ifstream stat_file;
  stat_file.open(stat_file_path);
  std::string line;
  std::getline(stat_file, line);
  std::getline(stat_file, line);
  while (std::getline(stat_file, line)) {
    std::istringstream iss(line);
    std::string entry_name;
    iss >> entry_name;
    if (iss.fail())
      continue;
    entry_name.pop_back();
    auto net_dev_data = NetDevData::parseNetDevLine(iss);
    if (net_dev_data.has_value())
      net_devs.emplace_back(entry_name, net_dev_data.value(), std::chrono::steady_clock::now());
  }
  return net_devs;
}

std::vector<DiskStat> ProcFs::getDiskStats() const {
  std::vector<DiskStat> disk_stats;
  auto disk_stats_file_path = root_path_ / DISK_STATS_FILE;
  std::ifstream stat_file;
  stat_file.open(disk_stats_file_path);
  std::string line;
  while (std::getline(stat_file, line)) {
    std::istringstream iss(line);
    auto disk_stat_data = DiskStatData::parseDiskStatLine(iss);
    if (disk_stat_data.has_value())
      disk_stats.emplace_back(disk_stat_data.value(), std::chrono::steady_clock::now());
  }
  return disk_stats;
}

}  // namespace org::apache::nifi::minifi::procfs
