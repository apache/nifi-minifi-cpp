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
