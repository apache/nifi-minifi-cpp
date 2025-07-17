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

#include "utils/ProcessCpuUsageTracker.h"

#ifndef WIN32
#include <sys/times.h>
#endif

#include <thread>
#include <algorithm>

namespace org::apache::nifi::minifi::utils {
#ifndef WIN32
ProcessCpuUsageTracker::ProcessCpuUsageTracker()
    : cpu_times_(0),
      sys_cpu_times_(0),
      user_cpu_times_(0),
      previous_cpu_times_(0),
      previous_sys_cpu_times_(0),
      previous_user_cpu_times_(0) {
  queryCpuTimes();
}

double ProcessCpuUsageTracker::getCpuUsageAndRestartCollection() {
  queryCpuTimes();
  if (isCurrentQuerySameAsPrevious() || isCurrentQueryOlderThanPrevious()) {
    return -1.0;
  } else {
    return getProcessCpuUsageBetweenLastTwoQueries();
  }
}

void ProcessCpuUsageTracker::queryCpuTimes() {
  previous_cpu_times_ = cpu_times_;
  previous_sys_cpu_times_ = sys_cpu_times_;
  previous_user_cpu_times_ = user_cpu_times_;

  struct tms timeSample{};
  cpu_times_ = times(&timeSample);
  sys_cpu_times_ = timeSample.tms_stime;
  user_cpu_times_ = timeSample.tms_utime;
}

bool ProcessCpuUsageTracker::isCurrentQueryOlderThanPrevious() const {
  return (cpu_times_ < previous_cpu_times_ ||
          sys_cpu_times_ < previous_sys_cpu_times_ ||
          user_cpu_times_ < previous_user_cpu_times_);
}

bool ProcessCpuUsageTracker::isCurrentQuerySameAsPrevious() const {
  return (cpu_times_ == previous_cpu_times_ &&
          sys_cpu_times_ == previous_sys_cpu_times_ &&
          user_cpu_times_ == previous_user_cpu_times_);
}

double ProcessCpuUsageTracker::getProcessCpuUsageBetweenLastTwoQueries() const {
  clock_t cpu_times_diff = cpu_times_ - previous_cpu_times_;
  if (cpu_times_diff == 0) {
    return -1.0;
  }
  clock_t sys_cpu_times_diff = sys_cpu_times_ - previous_sys_cpu_times_;
  clock_t user_cpu_times_diff = user_cpu_times_ - previous_user_cpu_times_;
  double percent = static_cast<double>(sys_cpu_times_diff + user_cpu_times_diff) / static_cast<double>(cpu_times_diff);
  percent = percent / (std::max)(uint32_t{1}, std::thread::hardware_concurrency());
  return percent;
}

#else

ProcessCpuUsageTracker::ProcessCpuUsageTracker() :
    cpu_times_(0), previous_cpu_times_(0),
    sys_cpu_times_(0), previous_sys_cpu_times_(0),
    user_cpu_times_(0), previous_user_cpu_times_(0) {
  self_ = GetCurrentProcess();
  queryCpuTimes();
}

double ProcessCpuUsageTracker::getCpuUsageAndRestartCollection() {
  queryCpuTimes();
  if (isCurrentQuerySameAsPrevious() || isCurrentQueryOlderThanPrevious()) {
    return -1.0;
  } else {
    return getProcessCpuUsageBetweenLastTwoQueries();
  }
}

bool ProcessCpuUsageTracker::isCurrentQueryOlderThanPrevious() const {
  return (cpu_times_ < previous_cpu_times_ ||
          sys_cpu_times_ < previous_sys_cpu_times_ ||
          user_cpu_times_ < previous_user_cpu_times_);
}

bool ProcessCpuUsageTracker::isCurrentQuerySameAsPrevious() const {
  return (cpu_times_ == previous_cpu_times_ &&
          sys_cpu_times_ == previous_sys_cpu_times_ &&
          user_cpu_times_ == previous_user_cpu_times_);
}

void ProcessCpuUsageTracker::queryCpuTimes() {
  previous_cpu_times_ = cpu_times_;
  previous_sys_cpu_times_ = sys_cpu_times_;
  previous_user_cpu_times_ = user_cpu_times_;
  FILETIME ftime, fsys, fuser;
  GetSystemTimeAsFileTime(&ftime);

  cpu_times_ = ULARGE_INTEGER{ ftime.dwLowDateTime, ftime.dwHighDateTime }.QuadPart;

  GetProcessTimes(self_, &ftime, &ftime, &fsys, &fuser);
  sys_cpu_times_ = ULARGE_INTEGER{ fsys.dwLowDateTime, fsys.dwHighDateTime }.QuadPart;
  user_cpu_times_ = ULARGE_INTEGER{ fuser.dwLowDateTime, fuser.dwHighDateTime }.QuadPart;
}

double ProcessCpuUsageTracker::getProcessCpuUsageBetweenLastTwoQueries() const {
  uint64_t cpu_times_diff = cpu_times_ - previous_cpu_times_;
  if (cpu_times_diff == 0) {
    return -1.0;
  }
  uint64_t sys_cpu_times_diff = sys_cpu_times_ - previous_sys_cpu_times_;
  uint64_t user_cpu_times_diff = user_cpu_times_ - previous_user_cpu_times_;
  double percent = static_cast<double>(sys_cpu_times_diff + user_cpu_times_diff) / static_cast<double>(cpu_times_diff);
  percent = percent / (std::max)(uint32_t{1}, std::thread::hardware_concurrency());
  return percent;
}
#endif

}  // namespace org::apache::nifi::minifi::utils
