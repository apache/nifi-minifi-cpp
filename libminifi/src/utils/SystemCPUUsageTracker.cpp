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

#include "utils/SystemCPUUsageTracker.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
#ifdef __linux__

SystemCPUUsageTracker::SystemCPUUsageTracker() :
    total_user_(0), previous_total_user_(0),
    total_user_low_(0), previous_total_user_low_(0),
    total_sys_(0), previous_total_sys_(0),
    total_idle_(0), previous_total_idle_(0) {
  queryCPUTimes();
}

double SystemCPUUsageTracker::getCPUUsageAndRestartCollection() {
  queryCPUTimes();
  if (isCurrentQuerySameAsPrevious() || isCurrentQuerySameAsPrevious()) {
    return -1.0;
  } else {
    return getCPUUsageBetweenLastTwoQueries();
  }
}

void SystemCPUUsageTracker::queryCPUTimes() {
  previous_total_user_ = total_user_;
  previous_total_user_low_ = total_user_low_;
  previous_total_sys_ = total_sys_;
  previous_total_idle_ = total_idle_;
  FILE* file = fopen("/proc/stat", "r");
  fscanf(file, "cpu %lu %lu %lu %lu", &total_user_, &total_user_low_,
         &total_sys_, &total_idle_);
  fclose(file);
}

bool SystemCPUUsageTracker::isCurrentQueryOlderThanPrevious() {
  return (total_user_ < previous_total_user_ ||
          total_user_low_ < previous_total_user_low_ ||
          total_sys_ < previous_total_sys_ ||
          total_idle_ < previous_total_idle_);
}

bool SystemCPUUsageTracker::isCurrentQuerySameAsPrevious() {
  return (total_user_ == previous_total_user_ &&
          total_user_low_ == previous_total_user_low_ &&
          total_sys_ == previous_total_sys_ &&
          total_idle_ == previous_total_idle_);
}

double SystemCPUUsageTracker::getCPUUsageBetweenLastTwoQueries() {
  double percent;

  uint64_t total_user_diff = total_user_ - previous_total_user_;
  uint64_t total_user_low_diff = total_user_low_ - previous_total_user_low_;
  uint64_t total_system_diff = total_sys_ - previous_total_sys_;
  uint64_t total_idle_diff = total_idle_ - previous_total_idle_;
  uint64_t total_diff =  total_user_diff + total_user_low_diff + total_system_diff;
  percent = static_cast<double>(total_diff)/static_cast<double>(total_diff+total_idle_diff);

  return percent;
}
#endif  // linux

#ifdef WIN32
SystemCPUUsageTracker::SystemCPUUsageTracker() :
    is_query_open_(false),
    cpu_query_(),
    cpu_total_() {
  openQuery();
}

SystemCPUUsageTracker::~SystemCPUUsageTracker() {
  PdhCloseQuery(cpu_query_);
}

double SystemCPUUsageTracker::getCPUUsageAndRestartCollection() {
  double value = getValueFromOpenQuery();
  return value;
}

void SystemCPUUsageTracker::openQuery() {
  if (!is_query_open_) {
    if (ERROR_SUCCESS != PdhOpenQuery(NULL, NULL, &cpu_query_))
      return;
    if (ERROR_SUCCESS != PdhAddEnglishCounter(cpu_query_, "\\Processor(_Total)\\% Processor Time", NULL, &cpu_total_)) {
      PdhCloseQuery(cpu_query_);
      return;
    }
    if (ERROR_SUCCESS != PdhCollectQueryData(cpu_query_)) {
      PdhCloseQuery(cpu_query_);
      return;
    }
    is_query_open_ = true;
  }
}

double SystemCPUUsageTracker::getValueFromOpenQuery() {
  if (!is_query_open_)
    return -1.0;

  PDH_FMT_COUNTERVALUE counterVal;
  if (ERROR_SUCCESS != PdhCollectQueryData(cpu_query_))
    return -1.0;
  if (ERROR_SUCCESS != PdhGetFormattedCounterValue(cpu_total_, PDH_FMT_DOUBLE, NULL, &counterVal))
    return -1.0;

  return counterVal.doubleValue / 100;
}
#endif  // windows

#ifdef __APPLE__
SystemCPUUsageTracker::SystemCPUUsageTracker() :
    total_ticks_(0), previous_total_ticks_(0),
    idle_ticks_(0), previous_idle_ticks_(0) {
  queryCPUTicks();
}

double SystemCPUUsageTracker::getCPUUsageAndRestartCollection() {
  queryCPUTicks();
  if (isCurrentQuerySameAsPrevious() || isCurrentQuerySameAsPrevious()) {
    return -1.0;
  } else {
    return getCPUUsageBetweenLastTwoQueries();
  }
}

void SystemCPUUsageTracker::queryCPUTicks() {
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

bool SystemCPUUsageTracker::isCurrentQueryOlderThanPrevious() {
  return (total_ticks_ < previous_total_ticks_ ||
          idle_ticks_ < previous_idle_ticks_);
}

bool SystemCPUUsageTracker::isCurrentQuerySameAsPrevious() {
  return (total_ticks_ == previous_total_ticks_ &&
          idle_ticks_ == previous_idle_ticks_);
}

double SystemCPUUsageTracker::getCPUUsageBetweenLastTwoQueries() {
  double percent;

  uint64_t total_ticks_since_last_time = total_ticks_-previous_total_ticks_;
  uint64_t idle_ticks_since_last_time  = idle_ticks_-previous_idle_ticks_;

  percent = static_cast<double>(total_ticks_since_last_time-idle_ticks_since_last_time)/static_cast<double>(total_ticks_since_last_time);

  return percent;
}
#endif  // macOS

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
