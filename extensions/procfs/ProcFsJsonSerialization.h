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

#include <string>
#include <string_view>

#include "ProcFsSerialization.h"
#include "rapidjson/stream.h"
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::extensions::procfs {

namespace details {
class Serializer {
 public:
  Serializer(rapidjson::Value& json, rapidjson::Document::AllocatorType& alloc) :
      json_(json), alloc_(alloc) {
  }
  void operator()(const char* key, uint64_t value) {
    json_.AddMember(rapidjson::StringRef(key), value, alloc_);
  }
  void operator()(const char* key, double value) {
    json_.AddMember(rapidjson::StringRef(key), value, alloc_);
  }
  void operator()(const char* key, std::string_view value) {
    rapidjson::Value value_json(value.data(), value.size(), alloc_);
    json_.AddMember(rapidjson::StringRef(key), value_json, alloc_);
  }
 private:
  rapidjson::Value& json_;
  rapidjson::Document::AllocatorType& alloc_;
};
}  // namespace details

void addCPUStatToJson(const std::string& cpu_name,
                      const CpuStatData& cpu_stat,
                      rapidjson::Value& cpu_root,
                      rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value cpu_key(cpu_name.c_str(), cpu_name.length(), alloc);
  cpu_root.AddMember(cpu_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& cpu_stat_json = cpu_root[cpu_name.c_str()];
  SerializeCPUStatData(cpu_stat,
                       details::Serializer(cpu_stat_json, alloc));
}

void addCPUStatPeriodToJson(const std::string& cpu_name,
                            const CpuStatData& start,
                            const CpuStatData& end,
                            rapidjson::Value& cpu_root,
                            rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value cpu_key(cpu_name.c_str(), cpu_name.length(), alloc);
  cpu_root.AddMember(cpu_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& cpu_stat_json = cpu_root[cpu_name.c_str()];
  SerializeNormalizedCPUStat(end-start,
                             details::Serializer(cpu_stat_json, alloc));
}

void addDiskStatToJson(const std::string& disk_name,
                       const DiskStatData& disk_stat,
                       rapidjson::Value& disk_root,
                       rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value disk_key(disk_name.c_str(), disk_name.length(), alloc);
  disk_root.AddMember(disk_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& disk_json = disk_root[disk_name.c_str()];
  SerializeDiskStatData(disk_stat,
                        details::Serializer(disk_json, alloc));
}

void addDiskStatPerSecToJson(const std::string& disk_name,
                             const DiskStatData& disk_stat,
                             const std::chrono::duration<double> duration,
                             rapidjson::Value& disk_root,
                             rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value disk_key(disk_name.c_str(), disk_name.length(), alloc);
  disk_root.AddMember(disk_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& disk_json = disk_root[disk_name.c_str()];
  SerializeDiskStatDataPerSec(disk_stat,
                              duration,
                              details::Serializer(disk_json, alloc));
}


void addNetDevToJson(const std::string& interface_name,
                     const NetDevData& net_dev,
                     rapidjson::Value& net_root,
                     rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value net_key(interface_name.c_str(), interface_name.length(), alloc);
  net_root.AddMember(net_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& net_dev_json = net_root[interface_name.c_str()];
  SerializeNetDevData(net_dev,
                      details::Serializer(net_dev_json, alloc));
}

void addNetDevPerSecToJson(const std::string& interface_name,
                           const NetDevData& net_dev,
                           const std::chrono::duration<double> duration,
                           rapidjson::Value& net_root,
                           rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value net_key(interface_name.c_str(), interface_name.length(), alloc);
  net_root.AddMember(net_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& net_dev_json = net_root[interface_name.c_str()];
  SerializeNetDevDataPerSec(net_dev,
                            duration,
                            details::Serializer(net_dev_json, alloc));
}


void addProcessStatToJson(const std::string& pid,
                          const ProcessStat& process_stat,
                          rapidjson::Value& process_root,
                          rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value process_key(pid.c_str(), pid.length(), alloc);
  process_root.AddMember(process_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& process_json = process_root[pid.c_str()];
  SerializeProcessStat(process_stat,
                       details::Serializer(process_json, alloc));
}

void addNormalizedProcessStatToJson(const std::string& pid,
                                    const ProcessStat& process_stat_start,
                                    const ProcessStat& process_stat_end,
                                    const std::chrono::duration<double> duration,
                                    rapidjson::Value& process_root,
                                    rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value process_key(pid.c_str(), pid.length(), alloc);
  process_root.AddMember(process_key.Move(), rapidjson::kObjectType, alloc);
  rapidjson::Value& process_json = process_root[pid.c_str()];
  SerializeNormalizedProcessStat(process_stat_start,
                                 process_stat_end,
                                 duration,
                                 details::Serializer(process_json, alloc));
}

void addMemInfoToJson(const MemInfo& mem_info,
                      rapidjson::Value& memory_root,
                      rapidjson::Document::AllocatorType& alloc) {
  SerializeMemInfo(mem_info, details::Serializer(memory_root, alloc));
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
