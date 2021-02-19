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
#ifndef LIBMINIFI_INCLUDE_UTILS_PROCESSCPUUSAGETRACKER_H_
#define LIBMINIFI_INCLUDE_UTILS_PROCESSCPUUSAGETRACKER_H_

#if defined(__linux__) || defined(__APPLE__)
#include <time.h>
#endif

#ifdef WIN32
#include <cstdint>
#include "windows.h"
#endif
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class ProcessCPUUsageTrackerBase {
 public:
  ProcessCPUUsageTrackerBase() = default;
  virtual ~ProcessCPUUsageTrackerBase() = default;
  virtual double getCPUUsageAndRestartCollection() = 0;
};

#if defined(__linux__) || defined(__APPLE__)
class ProcessCPUUsageTracker : ProcessCPUUsageTrackerBase {
 public:
  ProcessCPUUsageTracker();
  ~ProcessCPUUsageTracker() = default;
  double getCPUUsageAndRestartCollection() override;

 protected:
  void queryCPUTimes();
  bool isCurrentQueryOlderThanPrevious();
  bool isCurrentQuerySameAsPrevious();
  double getProcessCPUUsageBetweenLastTwoQueries();

 private:
  clock_t cpu_times_;
  clock_t sys_cpu_times_;
  clock_t user_cpu_times_;

  clock_t previous_cpu_times_;
  clock_t previous_sys_cpu_times_;
  clock_t previous_user_cpu_times_;
};
#endif  // linux, macOS

#if defined(WIN32)
class ProcessCPUUsageTracker : ProcessCPUUsageTrackerBase {
 public:
  ProcessCPUUsageTracker();
  ~ProcessCPUUsageTracker() = default;
  double getCPUUsageAndRestartCollection() override;

 protected:
  void queryCPUTimes();
  bool isCurrentQuerySameAsPrevious();
  bool isCurrentQueryOlderThanPrevious();
  double getProcessCPUUsageBetweenLastTwoQueries();

 private:
  HANDLE self_;
  uint64_t cpu_times_;
  uint64_t sys_cpu_times_;
  uint64_t user_cpu_times_;

  uint64_t previous_cpu_times_;
  uint64_t previous_sys_cpu_times_;
  uint64_t previous_user_cpu_times_;
};
#endif  // Windows

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_UTILS_PROCESSCPUUSAGETRACKER_H_
