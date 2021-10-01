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

#include <sstream>
#include <string>
#include <utility>
#include <optional>
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::procfs {

class CpuStatData {
  CpuStatData() = default;

 public:
  CpuStatData(const CpuStatData& src) = default;
  CpuStatData(CpuStatData&& src) noexcept = default;

  static std::optional<CpuStatData> parseCpuStatLine(std::istringstream& iss);

  bool operator>=(const CpuStatData& rhs) const;
  bool operator==(const CpuStatData& rhs) const;
  CpuStatData operator-(const CpuStatData& rhs) const;

  uint64_t getUser() const { return user_; }
  uint64_t getNice() const { return nice_; }
  uint64_t getSystem() const { return system_; }
  uint64_t getIdle() const { return idle_; }
  uint64_t getIoWait() const { return io_wait_; }
  uint64_t getIrq() const { return irq_; }
  uint64_t getSoftIrq() const { return soft_irq_; }
  uint64_t getSteal() const { return steal_; }
  uint64_t getGuest() const { return guest_; }
  uint64_t getGuestNice() const { return guest_nice_; }

  uint64_t getIdleAll() const { return idle_ + io_wait_; }
  uint64_t getSystemAll() const { return system_ + irq_ + soft_irq_; }
  uint64_t getVirtAll() const { return guest_ + guest_nice_; }
  uint64_t getTotal() const { return user_ + nice_ + getSystemAll() + getIdleAll() + steal_ + getVirtAll(); }

 private:
  uint64_t user_;
  uint64_t nice_;
  uint64_t system_;
  uint64_t idle_;
  uint64_t io_wait_;
  uint64_t irq_;
  uint64_t soft_irq_;
  uint64_t steal_;
  uint64_t guest_;
  uint64_t guest_nice_;
};

class CpuStat {
 public:
  static constexpr const char USER_STR[] = "user time";
  static constexpr const char NICE_STR[] = "nice time";
  static constexpr const char SYSTEM_STR[] = "system time";
  static constexpr const char IDLE_STR[] = "idle time";
  static constexpr const char IO_WAIT_STR[] = "io wait time";
  static constexpr const char IRQ_STR[] = "irq time";
  static constexpr const char SOFT_IRQ_STR[] = "soft irq time";
  static constexpr const char STEAL_STR[] = "steal time";
  static constexpr const char GUEST_STR[] = "guest time";
  static constexpr const char GUEST_NICE_STR[] = "guest nice time";

  CpuStat(const CpuStat& src) = default;
  CpuStat(CpuStat&& src) noexcept = default;

  CpuStat(std::string cpu_name, CpuStatData data) : cpu_name_(std::move(cpu_name)), data_(data) {}

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const;
  const CpuStatData& getData() const { return data_; }
  const std::string& getName() const { return cpu_name_; }

 protected:
  std::string cpu_name_;
  CpuStatData data_;
};

class CpuStatPeriod {
  CpuStatPeriod(std::string cpu_name, double period, CpuStatData data_diff) : cpu_name_(std::move(cpu_name)), period_(period), data_diff_(data_diff) {}

 public:
  static constexpr const char USER_PERCENT_STR[] = "user time %";
  static constexpr const char NICE_PERCENT_STR[] = "nice time %";
  static constexpr const char SYSTEM_PERCENT_STR[] = "system time %";
  static constexpr const char IDLE_PERCENT_STR[] = "idle time %";
  static constexpr const char IO_WAIT_PERCENT_STR[] = "io wait time %";
  static constexpr const char IRQ_PERCENT_STR[] = "irq time %";
  static constexpr const char SOFT_IRQ_PERCENT_STR[] = "soft irq %";
  static constexpr const char STEAL_PERCENT_STR[] = "steal time %";
  static constexpr const char GUEST_PERCENT_STR[] = "guest time %";
  static constexpr const char GUEST_NICE_PERCENT_STR[] = "guest nice time %";

  CpuStatPeriod(const CpuStatPeriod& src) = default;
  CpuStatPeriod(CpuStatPeriod&& src) noexcept = default;

  static std::optional<CpuStatPeriod> create(const CpuStat& cpu_stat_start, const CpuStat& cpu_stat_end);

  void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const;

  double getPeriod() const { return period_; }
  const std::string& getName() const { return cpu_name_; }

  double getUserPercent() const { return static_cast<double>(data_diff_.getUser())/ period_; }
  double getNicePercent() const { return static_cast<double>(data_diff_.getNice())/ period_; }
  double getSystemPercent() const { return static_cast<double>(data_diff_.getSystem())/ period_; }
  double getIdlePercent() const { return static_cast<double>(data_diff_.getIdle())/ period_; }
  double getIoWaitPercent() const { return static_cast<double>(data_diff_.getIoWait()) / period_; }
  double getIrqPercent() const { return static_cast<double>(data_diff_.getIrq())/ period_; }
  double getSoftIrqPercent() const { return static_cast<double>(data_diff_.getSoftIrq()) / period_; }
  double getStealPercent() const { return static_cast<double>(data_diff_.getSteal())/ period_; }
  double getGuestPercent() const { return static_cast<double>(data_diff_.getGuest())/ period_; }
  double getGuestNicePercent() const { return static_cast<double>(data_diff_.getGuestNice())/ period_; }

 protected:
  std::string cpu_name_;
  double period_;
  CpuStatData data_diff_;
};

}  // namespace org::apache::nifi::minifi::procfs

