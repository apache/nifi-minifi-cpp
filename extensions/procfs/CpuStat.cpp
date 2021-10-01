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

#include "CpuStat.h"

namespace org::apache::nifi::minifi::procfs {

std::optional<CpuStatData> CpuStatData::parseCpuStatLine(std::istringstream& iss) {
  CpuStatData data;
  iss >> data.user_ >> data.nice_ >> data.system_ >> data.idle_ >> data.io_wait_ >> data.irq_ >> data.soft_irq_ >> data.steal_ >> data.guest_ >> data.guest_nice_;
  if (iss.fail())
    return std::nullopt;
  data.user_ -= data.guest_;  // Guest time is already accounted in usertime
  data.nice_ -= data.guest_nice_;
  return data;
}

bool CpuStatData::operator>=(const CpuStatData& rhs) const {
  return user_ >= rhs.user_
         && nice_ >= rhs.nice_
         && system_ >= rhs.system_
         && idle_ >= rhs.idle_
         && io_wait_ >= rhs.io_wait_
         && irq_ >= rhs.irq_
         && soft_irq_ >= rhs.soft_irq_
         && steal_ >= rhs.steal_
         && guest_ >= rhs.guest_
         && guest_nice_ >= rhs.guest_nice_;
}

bool CpuStatData::operator==(const CpuStatData& rhs) const {
  return user_ == rhs.user_
         && nice_ == rhs.nice_
         && system_ == rhs.system_
         && idle_ == rhs.idle_
         && io_wait_ == rhs.io_wait_
         && irq_ == rhs.irq_
         && soft_irq_ == rhs.soft_irq_
         && steal_ == rhs.steal_
         && guest_ == rhs.guest_
         && guest_nice_ == rhs.guest_nice_;
}

CpuStatData CpuStatData::operator-(const CpuStatData& rhs) const {
  CpuStatData result;
  result.user_ = user_ - rhs.user_;
  result.nice_ = nice_ - rhs.nice_;
  result.system_ = system_ - rhs.system_;
  result.idle_ = idle_ - rhs.idle_;
  result.io_wait_ = io_wait_ - rhs.io_wait_;
  result.irq_ = irq_ - rhs.irq_;
  result.soft_irq_ = soft_irq_ - rhs.soft_irq_;
  result.steal_ = steal_ - rhs.steal_;
  result.guest_ = guest_ - rhs.guest_;
  result.guest_nice_ = guest_nice_ - rhs.guest_nice_;
  return result;
}

void CpuStat::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value cpu_name_json(cpu_name_.c_str(), cpu_name_.length(), alloc);
  body.AddMember(cpu_name_json, rapidjson::kObjectType, alloc);
  rapidjson::Value& cpu_stat_json = body[cpu_name_.c_str()];
  cpu_stat_json.AddMember(USER_STR, data_.getUser(), alloc);
  cpu_stat_json.AddMember(NICE_STR, data_.getNice(), alloc);
  cpu_stat_json.AddMember(SYSTEM_STR, data_.getSystem(), alloc);
  cpu_stat_json.AddMember(IDLE_STR, data_.getIdle(), alloc);
  cpu_stat_json.AddMember(IO_WAIT_STR, data_.getIoWait(), alloc);
  cpu_stat_json.AddMember(IRQ_STR, data_.getIrq(), alloc);
  cpu_stat_json.AddMember(SOFT_IRQ_STR, data_.getSoftIrq(), alloc);
  cpu_stat_json.AddMember(STEAL_STR, data_.getSteal(), alloc);
  cpu_stat_json.AddMember(GUEST_STR, data_.getGuest(), alloc);
  cpu_stat_json.AddMember(GUEST_NICE_STR, data_.getGuestNice(), alloc);
}

void CpuStatPeriod::addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const {
  rapidjson::Value cpu_name_json(cpu_name_.c_str(), cpu_name_.length(), alloc);
  body.AddMember(cpu_name_json, rapidjson::kObjectType, alloc);
  rapidjson::Value& cpu_stat_json = body[cpu_name_.c_str()];
  cpu_stat_json.AddMember(USER_PERCENT_STR, getUserPercent(), alloc);
  cpu_stat_json.AddMember(NICE_PERCENT_STR, getNicePercent(), alloc);
  cpu_stat_json.AddMember(SYSTEM_PERCENT_STR, getSystemPercent(), alloc);
  cpu_stat_json.AddMember(IDLE_PERCENT_STR, getIdlePercent(), alloc);
  cpu_stat_json.AddMember(IO_WAIT_PERCENT_STR, getIoWaitPercent(), alloc);
  cpu_stat_json.AddMember(IRQ_PERCENT_STR, getIrqPercent(), alloc);
  cpu_stat_json.AddMember(SOFT_IRQ_PERCENT_STR, getSoftIrqPercent(), alloc);
  cpu_stat_json.AddMember(STEAL_PERCENT_STR, getStealPercent(), alloc);
  cpu_stat_json.AddMember(GUEST_PERCENT_STR, getGuestPercent(), alloc);
  cpu_stat_json.AddMember(GUEST_NICE_PERCENT_STR, getGuestNicePercent(), alloc);
}

std::optional<CpuStatPeriod> CpuStatPeriod::create(const CpuStat& cpu_stat_start, const CpuStat& cpu_stat_end) {
  if (cpu_stat_start.getName() != cpu_stat_end.getName())
    return std::nullopt;
  if (cpu_stat_start.getData() == cpu_stat_end.getData())
    return std::nullopt;
  if (!(cpu_stat_end.getData() >= cpu_stat_start.getData()))
    return std::nullopt;
  return CpuStatPeriod(cpu_stat_start.getName(), cpu_stat_end.getData().getTotal()-cpu_stat_start.getData().getTotal(), cpu_stat_end.getData()-cpu_stat_start.getData());
}


}  // namespace org::apache::nifi::minifi::procfs
