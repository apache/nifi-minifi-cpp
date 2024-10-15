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

#include "utils/SystemCpuUsageTracker.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils {
#ifdef __linux__

SystemCpuUsageTracker::SystemCpuUsageTracker() :
    total_user_(0), previous_total_user_(0),
    total_user_low_(0), previous_total_user_low_(0),
    total_sys_(0), previous_total_sys_(0),
    total_idle_(0), previous_total_idle_(0) {
  queryCpuTimes();
}

double SystemCpuUsageTracker::getCpuUsageAndRestartCollection() {
  queryCpuTimes();
  if (isCurrentQuerySameAsPrevious() || isCurrentQueryOlderThanPrevious()) {
    return -1.0;
  } else {
    return getCpuUsageBetweenLastTwoQueries();
  }
}

void SystemCpuUsageTracker::queryCpuTimes() {
  previous_total_user_ = total_user_;
  previous_total_user_low_ = total_user_low_;
  previous_total_sys_ = total_sys_;
  previous_total_idle_ = total_idle_;
  gsl::owner<FILE*> file = fopen("/proc/stat", "r");
  if (fscanf(file, "cpu %lu %lu %lu %lu", &total_user_, &total_user_low_, &total_sys_, &total_idle_) != 4) {  // NOLINT(cert-err34-c)
    total_user_ = previous_total_user_;
    total_user_low_ = previous_total_user_low_;
    total_idle_ = previous_total_idle_;
    total_sys_ = previous_total_sys_;
  }
  (void)fclose(file);
}

bool SystemCpuUsageTracker::isCurrentQueryOlderThanPrevious() const {
  return (total_user_ < previous_total_user_ ||
          total_user_low_ < previous_total_user_low_ ||
          total_sys_ < previous_total_sys_ ||
          total_idle_ < previous_total_idle_);
}

bool SystemCpuUsageTracker::isCurrentQuerySameAsPrevious() const {
  return (total_user_ == previous_total_user_ &&
          total_user_low_ == previous_total_user_low_ &&
          total_sys_ == previous_total_sys_ &&
          total_idle_ == previous_total_idle_);
}

double SystemCpuUsageTracker::getCpuUsageBetweenLastTwoQueries() const {
  uint64_t total_user_diff = total_user_ - previous_total_user_;
  uint64_t total_user_low_diff = total_user_low_ - previous_total_user_low_;
  uint64_t total_system_diff = total_sys_ - previous_total_sys_;
  uint64_t total_idle_diff = total_idle_ - previous_total_idle_;
  uint64_t total_diff = total_user_diff + total_user_low_diff + total_system_diff;
  if (total_diff + total_idle_diff == 0) {
    return -1.0;
  }
  double percent = static_cast<double>(total_diff) / static_cast<double>(total_diff + total_idle_diff);

  return percent;
}
#endif  // linux

#ifdef WIN32
SystemCpuUsageTracker::SystemCpuUsageTracker() :
    total_user_(0), previous_total_user_(0),
    total_sys_(0), previous_total_sys_(0),
    total_idle_(0), previous_total_idle_(0) {
  queryCpuTimes();
}

double SystemCpuUsageTracker::getCpuUsageAndRestartCollection() {
  queryCpuTimes();
  if (isCurrentQuerySameAsPrevious() || isCurrentQueryOlderThanPrevious()) {
    return -1.0;
  } else {
    return getCpuUsageBetweenLastTwoQueries();
  }
}

void SystemCpuUsageTracker::queryCpuTimes() {
  previous_total_user_ = total_user_;
  previous_total_sys_ = total_sys_;
  previous_total_idle_ = total_idle_;
  FILETIME fidle, fsys, fuser;
  GetSystemTimes(&fidle, &fsys, &fuser);
  total_user_ = ULARGE_INTEGER{ fuser.dwLowDateTime, fuser.dwHighDateTime }.QuadPart;
  total_sys_ = ULARGE_INTEGER{ fsys.dwLowDateTime, fsys.dwHighDateTime }.QuadPart;
  total_idle_ = ULARGE_INTEGER{ fidle.dwLowDateTime, fidle.dwHighDateTime }.QuadPart;
}

bool SystemCpuUsageTracker::isCurrentQueryOlderThanPrevious() const {
  return (total_user_ < previous_total_user_ ||
    total_sys_ < previous_total_sys_ ||
    total_idle_ < previous_total_idle_);
}

bool SystemCpuUsageTracker::isCurrentQuerySameAsPrevious() const {
  return (total_user_ == previous_total_user_ &&
    total_sys_ == previous_total_sys_ &&
    total_idle_ == previous_total_idle_);
}

double SystemCpuUsageTracker::getCpuUsageBetweenLastTwoQueries() const {
  uint64_t total_user_diff = total_user_ - previous_total_user_;
  uint64_t total_sys_diff = total_sys_ - previous_total_sys_;
  uint64_t total_idle_diff = total_idle_ - previous_total_idle_;
  uint64_t total_diff = total_user_diff + total_sys_diff;
  if (total_diff == 0) {
    return -1.0;
  }
  double percent = static_cast<double>(total_diff - total_idle_diff) / static_cast<double>(total_diff);

  return percent;
}
#endif  // windows

#ifdef __APPLE__
SystemCpuUsageTracker::SystemCpuUsageTracker() :
    total_ticks_(0), previous_total_ticks_(0),
    idle_ticks_(0), previous_idle_ticks_(0) {
  queryCpuTicks();
}

double SystemCpuUsageTracker::getCpuUsageAndRestartCollection() {
  queryCpuTicks();
  if (isCurrentQuerySameAsPrevious() || isCurrentQueryOlderThanPrevious()) {
    return -1.0;
  } else {
    return getCpuUsageBetweenLastTwoQueries();
  }
}

void SystemCpuUsageTracker::queryCpuTicks() {
  host_cpu_load_info_data_t cpuinfo;
  mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
  auto query_result = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpuinfo, &count);
  if (query_result == KERN_SUCCESS) {
    previous_total_ticks_ = total_ticks_;
    previous_idle_ticks_ = idle_ticks_;
    total_ticks_ = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) {
      total_ticks_ += cpuinfo.cpu_ticks[i];
    }
    idle_ticks_ = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
  }
}

bool SystemCpuUsageTracker::isCurrentQueryOlderThanPrevious() const {
  return (total_ticks_ < previous_total_ticks_ ||
          idle_ticks_ < previous_idle_ticks_);
}

bool SystemCpuUsageTracker::isCurrentQuerySameAsPrevious() const {
  return (total_ticks_ == previous_total_ticks_ &&
          idle_ticks_ == previous_idle_ticks_);
}

double SystemCpuUsageTracker::getCpuUsageBetweenLastTwoQueries() const {
  uint64_t total_ticks_since_last_time = total_ticks_-previous_total_ticks_;
  uint64_t idle_ticks_since_last_time  = idle_ticks_-previous_idle_ticks_;
  if (total_ticks_since_last_time == 0) {
    return -1.0;
  }
  double percent = static_cast<double>(total_ticks_since_last_time - idle_ticks_since_last_time) / static_cast<double>(total_ticks_since_last_time);

  return percent;
}
#endif  // macOS

}  // namespace org::apache::nifi::minifi::utils
