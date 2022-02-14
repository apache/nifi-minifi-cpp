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
#include <istream>

namespace org::apache::nifi::minifi::extensions::procfs {

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
      if (auto process_stat_data = ProcessStatData::parseProcessStatFile(stat_file)) {
        process_stats.emplace(process_stat_data->getPid(), ProcessStat(*process_stat_data, page_size_));
      }
    }
  }
  return process_stats;
}

std::unordered_map<std::string, CpuStatData> ProcFs::getCpuStats() const {
  std::unordered_map<std::string, CpuStatData> cpu_stats;
  auto stat_file_path = root_path_ / STAT_FILE;
  std::ifstream stat_file;
  stat_file.open(stat_file_path);
  std::string line;
  while (std::getline(stat_file, line)) {
    std::istringstream iss(line);
    std::string entry_name;
    iss >> entry_name;
    if (entry_name.starts_with("cpu")) {
      if (auto cpu_stat_data = CpuStatData::parseCpuStatLine(iss))
        cpu_stats.emplace(entry_name, *cpu_stat_data);
    }
  }
  return cpu_stats;
}

std::optional<MemInfo> ProcFs::getMemInfo() const {
  auto mem_info_file_path = root_path_ / MEMINFO_FILE;
  std::ifstream mem_info_file(mem_info_file_path);
  return MemInfo::parseMemInfoFile(mem_info_file);
}

std::unordered_map<std::string, NetDevData> ProcFs::getNetDevs() const {
  std::unordered_map<std::string, NetDevData>net_devs;
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
    if (auto net_dev_data = NetDevData::parseNetDevLine(iss))
      net_devs.emplace(entry_name, *net_dev_data);
  }
  return net_devs;
}

std::unordered_map<std::string, DiskStatData> ProcFs::getDiskStats() const {
  std::unordered_map<std::string, DiskStatData> disk_stats;
  auto disk_stats_file_path = root_path_ / DISK_STATS_FILE;
  std::ifstream stat_file;
  stat_file.open(disk_stats_file_path);
  std::string line;
  while (std::getline(stat_file, line)) {
    std::istringstream iss(line);
    if (auto disk_stat = DiskStatData::parseDiskStatLine(iss))
      disk_stats.emplace(disk_stat->first, disk_stat->second);
  }
  return disk_stats;
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
