// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <istream>
#include <optional>
#include <string>

#include "SystemClockDuration.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::extensions::procfs {

class CpuStatData {
 private:
  CpuStatData() = default;

 public:
  static std::optional<CpuStatData> parseCpuStatLine(std::istream& iss);

  auto operator<=>(const CpuStatData& rhs) const = default;
  CpuStatData operator-(const CpuStatData& rhs) const;

  [[nodiscard]] SystemClockDuration getUser() const noexcept { return user_; }
  [[nodiscard]] SystemClockDuration getNice() const noexcept { return nice_; }
  [[nodiscard]] SystemClockDuration getSystem() const noexcept { return system_; }
  [[nodiscard]] SystemClockDuration getIdle() const noexcept { return idle_; }
  [[nodiscard]] SystemClockDuration getIoWait() const noexcept { return io_wait_; }
  [[nodiscard]] SystemClockDuration getIrq() const noexcept { return irq_; }
  [[nodiscard]] SystemClockDuration getSoftIrq() const noexcept { return soft_irq_; }
  [[nodiscard]] SystemClockDuration getSteal() const noexcept { return steal_; }
  [[nodiscard]] SystemClockDuration getGuest() const noexcept { return guest_; }
  [[nodiscard]] SystemClockDuration getGuestNice() const noexcept { return guest_nice_; }

  [[nodiscard]] SystemClockDuration getIdleAll() const noexcept { return idle_ + io_wait_; }
  [[nodiscard]] SystemClockDuration getSystemAll() const noexcept { return system_ + irq_ + soft_irq_; }
  [[nodiscard]] SystemClockDuration getVirtAll() const noexcept { return guest_ + guest_nice_; }
  [[nodiscard]] SystemClockDuration getTotal() const noexcept { return user_ + nice_ + getSystemAll() + getIdleAll() + steal_; }  // VirtAll is already included in User and Nice

 private:
  SystemClockDuration user_;
  SystemClockDuration nice_;
  SystemClockDuration system_;
  SystemClockDuration idle_;
  SystemClockDuration io_wait_;
  SystemClockDuration irq_;
  SystemClockDuration soft_irq_;
  SystemClockDuration steal_;
  SystemClockDuration guest_;
  SystemClockDuration guest_nice_;
};
}  // namespace org::apache::nifi::minifi::extensions::procfs

