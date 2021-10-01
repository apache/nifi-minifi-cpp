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

#include <sys/types.h>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <optional>
#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::procfs {

class ProcessStatData {
  ProcessStatData() = default;

 public:
  static std::optional<ProcessStatData> parseProcessStatFile(std::ifstream& stat_file);

  pid_t getPid() const { return pid_; }
  const std::string& getComm() const { return comm_; }
  uint64_t getUtime() const { return utime_; }
  uint64_t getStime() const { return stime_; }
  uint64_t getRss() const {return rss_; }

 private:
  pid_t pid_                          = -1;  // (1) The process ID.
  std::string comm_                   = "";  // (2) The filename of the executable, in parentheses. This is visible whether or not the executable is swapped out.
  char state_                         = 'R';  // (3) One character indicating the process state
  pid_t ppid_                         = -1;  // (4) The PID of the parent.
  pid_t pgrp_                         = -1;  // (5) The process group ID of the process.
  int32_t session_                    = 0;   // (6) The session ID of the process.
  int32_t tty_nr_                     = 0;   // (7) The controlling terminal of the process.
  pid_t tpgid_                        = -1;  // (8) The ID of the foreground process group of the controlling terminal of the process.
  uint32_t flags_                     = 0;   // (9) The kernel flags word of the process.
  uint64_t minflt_                    = 0;   // (10) The number of minor faults the process has made which have not required loading a memory page from disk.
  uint64_t cminflt_                   = 0;   // (11) The number of minor faults that the process's waited-for children have made.
  uint64_t majflt_                    = 0;   // (12) The number of major faults the process has made which have required loading a memory page from disk.
  uint64_t cmajflt_                   = 0;   // (13) The number of major faults that the process's waited-for children have made.
  uint64_t utime_                     = 0;   // (14) Amount of time that this process has been scheduled in user mode, measured in clock ticks.
  uint64_t stime_                     = 0;   // (15) Amount of time that this process has been scheduled in kernel mode, measured in clock ticks.
  int32_t cutime_                     = 0;   // (16) Amount of time that this process's waited-for children have been scheduled in user mode, measured in clock ticks.
  int32_t cstime_                     = 0;   // (17) Amount of time that this process's waited-for children have been scheduled in kernel mode, measured in clock ticks.
  int32_t priority_                   = 0;   // (18) Priority
  int32_t nice_                       = 0;   // (19) The nice value.
  int32_t num_threads_                = 0;   // (20) Number of threads in this process
  int32_t itrealvalue_                = 0;   // (21) The time in jiffies before the next SIGALRM is sent to the process due to an interval timer.
  uint64_t starttime_                 = 0;   // (22) The time the process started after system boot.
  uint64_t vsize_                     = 0;   // (23) Virtual memory size in bytes.
  int32_t rss_                        = 0;   // (24) Resident Set Size: number of pages the process has in real memory.
  uint64_t rsslim_                    = 0;   // (25) Current soft limit in bytes on the rss of the process.
  uint64_t startcode_                 = 0;   // (26) The address above which program text can run.
  uint64_t endcode_                   = 0;   // (27) The address below which program text can run.
  uint64_t startstack_                = 0;   // (28) The address of the start (i.e., bottom) of the stack.
  uint64_t kstkesp_                   = 0;   // (29) The current value of ESP (stack pointer), as found in the kernel stack page for the process.
  uint64_t kstkeip_                   = 0;   // (30) The current EIP (instruction pointer).
  uint64_t signal_                    = 0;   // (31) The bitmap of pending signals, displayed as a decimal number.
  uint64_t blocked_                   = 0;   // (32) The bitmap of blocked signals, displayed as a decimal number.
  uint64_t sigignore_                 = 0;   // (33) The bitmap of ignored signals, displayed as a decimal number.
  uint64_t sigcatch_                  = 0;   // (34) The bitmap of caught signals, displayed as a decimal number.
  uint64_t wchan_                     = 0;   // (35) This is the "channel" in which the process is waiting.
  uint64_t nswap_                     = 0;   // (36) Number of pages swapped (not maintained).
  uint64_t cnswap_                    = 0;   // (37) Cumulative nswap for child processes (not maintained).
  int32_t exit_signal_                = 0;   // (38) Signal to be sent to parent when we die.
  int32_t processor_                  = 0;   // (39) CPU number last executed on.
  uint32_t rt_priority_               = 0;   // (40) Real-time scheduling priority, a number in the range 1 to 99 for processes scheduled under a realtime policy, or 0, for non-real-time processes
  uint32_t policy_                    = 0;   // (41) Scheduling policy
  uint64_t delayacct_blkio_ticks_     = 0;   // (42) Aggregated block I/O delays, measured in clock ticks (centiseconds).
  uint64_t guest_time_                = 0;   // (43) Guest time of the process (time spent running a virtual CPU for a guest operating system), measured in clock ticks
  int32_t cguest_time_                = 0;   // (44) Guest time of the process's children, measured in clock ticks
  uint64_t start_data_                = 0;   // (45) Address above which program initialized and uninitialized (BSS) data are placed.
  uint64_t end_data_                  = 0;   // (46) Address below which program initialized and uninitialized (BSS) data are placed.
  uint64_t start_brk_                 = 0;   // (47) Address above which program heap can be expanded with brk(2).
  uint64_t arg_start_                 = 0;   // (48) Address above which program command-line arguments (argv) are placed
  uint64_t arg_end_                   = 0;   // (49) Address below program command-line arguments (argv) are placed.
  uint64_t env_start_                 = 0;   // (50) Address above which program environment is placed.
  uint64_t env_end_                   = 0;   // (51) Address below which program environment is placed.
  int32_t exit_code_                  = 0;   // (52) The thread's exit status in the form reported by waitpid(2).
};

class ProcessStat {
 public:
  static constexpr const char COMM_STR[] = "COMM";
  static constexpr const char MEMORY_STR[] = "RES";
  static constexpr const char CPU_TIME_STR[] = "CPUTIME";

  explicit ProcessStat(const ProcessStatData& data, int page_size)
      : pid_(data.getPid()),
        comm_(data.getComm()),
        memory_(data.getRss()*page_size),
        cpu_time_(data.getUtime() + data.getStime()) {
  }

  void addToJson(rapidjson::Value& root, rapidjson::Document::AllocatorType& alloc) const;
  pid_t getPid() const { return pid_; }
  const std::string& getComm() const { return comm_; }
  uint64_t getMemory() const { return memory_; }
  uint64_t getCpuTime() const { return cpu_time_; }
 protected:
  pid_t pid_;
  std::string comm_;
  uint64_t memory_;
  uint64_t cpu_time_;
};


class ProcessStatPeriod {
  ProcessStatPeriod() = default;
 public:
  static constexpr const char COMM_STR[] = "COMM";
  static constexpr const char MEMORY_STR[] = "RES";
  static constexpr const char CPU_USAGE_STR[] = "CPU%";

  ProcessStatPeriod(const ProcessStatPeriod& src) = default;
  ProcessStatPeriod(ProcessStatPeriod&& src) noexcept = default;

  static std::optional<ProcessStatPeriod> create(const ProcessStat& process_stat_start, const ProcessStat& process_stat_end, double cpu_period);

  void addToJson(rapidjson::Value& root, rapidjson::Document::AllocatorType& alloc) const;

 protected:
  pid_t pid_;
  std::string comm_;
  uint64_t memory_;
  double cpu_usage_;
};

}  // namespace org::apache::nifi::minifi::procfs
