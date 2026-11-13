// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once
#include <asm-generic/param.h>
#include <istream>
#include <chrono>
#include "utils/TimeUtil.h"  // for libc++ duration operator<=> workaround

namespace org::apache::nifi::minifi::extensions::procfs {

typedef std::chrono::duration<uint64_t, std::ratio<1, HZ>> SystemClockDuration;

inline std::istream& operator>>(std::istream& iss, SystemClockDuration& system_duration) {
  uint64_t value;
  iss >> value;
  system_duration = SystemClockDuration(value);
  return iss;
}

}  // namespace org::apache::nifi::minifi::extensions::procfs
