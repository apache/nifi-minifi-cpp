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

#ifdef WIN32
#include <cstdint>
#include "windows.h"
#else
#include <time.h>
#endif

namespace org::apache::nifi::minifi::utils {

class ProcessCpuUsageTrackerBase {
 public:
  ProcessCpuUsageTrackerBase() = default;
  virtual ~ProcessCpuUsageTrackerBase() = default;
  virtual double getCpuUsageAndRestartCollection() = 0;
};

#ifndef WIN32
class ProcessCpuUsageTracker : ProcessCpuUsageTrackerBase {
 public:
  ProcessCpuUsageTracker();
  ~ProcessCpuUsageTracker() = default;
  double getCpuUsageAndRestartCollection() override;

 protected:
  void queryCpuTimes();
  bool isCurrentQueryOlderThanPrevious() const;
  bool isCurrentQuerySameAsPrevious() const;
  double getProcessCpuUsageBetweenLastTwoQueries() const;

 private:
  clock_t cpu_times_;
  clock_t sys_cpu_times_;
  clock_t user_cpu_times_;

  clock_t previous_cpu_times_;
  clock_t previous_sys_cpu_times_;
  clock_t previous_user_cpu_times_;
};

#else

class ProcessCpuUsageTracker : ProcessCpuUsageTrackerBase {
 public:
  ProcessCpuUsageTracker();
  ~ProcessCpuUsageTracker() = default;
  double getCpuUsageAndRestartCollection() override;

 protected:
  void queryCpuTimes();
  bool isCurrentQuerySameAsPrevious() const;
  bool isCurrentQueryOlderThanPrevious() const;
  double getProcessCpuUsageBetweenLastTwoQueries() const;

 private:
  HANDLE self_;
  uint64_t cpu_times_;
  uint64_t sys_cpu_times_;
  uint64_t user_cpu_times_;

  uint64_t previous_cpu_times_;
  uint64_t previous_sys_cpu_times_;
  uint64_t previous_user_cpu_times_;
};
#endif

}  // namespace org::apache::nifi::minifi::utils
