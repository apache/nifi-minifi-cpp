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

#include <cstdint>

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#endif

#ifdef WIN32
#include "TCHAR.h"
#include "windows.h"
#endif

#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#endif

namespace org::apache::nifi::minifi::utils {

class SystemCpuUsageTrackerBase {
 public:
  SystemCpuUsageTrackerBase() = default;
  virtual ~SystemCpuUsageTrackerBase() = default;
  virtual double getCpuUsageAndRestartCollection() = 0;
};

#ifdef __linux__
class SystemCpuUsageTracker : public SystemCpuUsageTrackerBase {
 public:
  SystemCpuUsageTracker();
  ~SystemCpuUsageTracker() = default;
  double getCpuUsageAndRestartCollection() override;

 protected:
  void queryCpuTimes();
  bool isCurrentQuerySameAsPrevious() const;
  bool isCurrentQueryOlderThanPrevious() const;
  double getCpuUsageBetweenLastTwoQueries() const;

 private:
  uint64_t total_user_;
  uint64_t previous_total_user_;

  uint64_t total_user_low_;
  uint64_t previous_total_user_low_;

  uint64_t total_sys_;
  uint64_t previous_total_sys_;

  uint64_t total_idle_;
  uint64_t previous_total_idle_;
};
#endif  // linux

#ifdef WIN32
class SystemCpuUsageTracker : public SystemCpuUsageTrackerBase {
 public:
  SystemCpuUsageTracker();
  ~SystemCpuUsageTracker() = default;
  double getCpuUsageAndRestartCollection() override;

 protected:
  void queryCpuTimes();
  bool isCurrentQuerySameAsPrevious() const;
  bool isCurrentQueryOlderThanPrevious() const;
  double getCpuUsageBetweenLastTwoQueries() const;

 private:
  uint64_t total_idle_;
  uint64_t total_sys_;
  uint64_t total_user_;

  uint64_t previous_total_idle_;
  uint64_t previous_total_sys_;
  uint64_t previous_total_user_;
};
#endif  // windows

#ifdef __APPLE__
class SystemCpuUsageTracker : public SystemCpuUsageTrackerBase {
 public:
  SystemCpuUsageTracker();
  ~SystemCpuUsageTracker() = default;
  double getCpuUsageAndRestartCollection() override;

 protected:
  void queryCpuTicks();
  bool isCurrentQueryOlderThanPrevious() const;
  bool isCurrentQuerySameAsPrevious() const;
  double getCpuUsageBetweenLastTwoQueries() const;

 private:
  uint64_t total_ticks_;
  uint64_t previous_total_ticks_;

  uint64_t idle_ticks_;
  uint64_t previous_idle_ticks_;
};
#endif  // macOS

}  // namespace org::apache::nifi::minifi::utils
