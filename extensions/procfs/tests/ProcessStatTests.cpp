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

#include "Catch.h"
#include "ProcFs.h"
#include "MockProcFs.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

TEST_CASE("ProcFSTest process test with mock", "[procfsprocessmocktest]") {
  ProcFs proc_fs_t0 = mock_proc_fs_t0();
  auto process_stats_t0 = proc_fs_t0.getProcessStats();
  REQUIRE(process_stats_t0.size() == 1);
  REQUIRE(process_stats_t0.contains(624372));
  CHECK(process_stats_t0.at(624372).getComm() == "gnome-system-mo");
  CHECK(process_stats_t0.at(624372).getCpuTime() == SystemClockDuration(1558+221));
  CHECK(process_stats_t0.at(624372).getMemory() == (14983UL)*sysconf(_SC_PAGESIZE));
  ProcFs proc_fs_t1 = mock_proc_fs_t1();
  auto process_stats_t1 = proc_fs_t1.getProcessStats();
  REQUIRE(process_stats_t1.size() == 1);
}

TEST_CASE("ProcFSTest process test", "[procfsprocesstest]") {
  ProcFs proc_fs;
  auto process_stats_t0 = proc_fs.getProcessStats();
  std::this_thread::sleep_for(100ms);
  auto process_stats_t1 = proc_fs.getProcessStats();
  size_t number_of_valid_measurements = 0;
  for (const auto& [pid_t0, process_stat_t0] : process_stats_t0) {
    if (process_stats_t1.contains(pid_t0)) {
      const auto& process_stat_t1 = process_stats_t1.at(pid_t0);
      if (process_stat_t1.getComm() != process_stat_t0.getComm())
        continue;
      ++number_of_valid_measurements;
      REQUIRE(process_stat_t1.getCpuTime() >= process_stat_t0.getCpuTime());
    }
  }
  REQUIRE(number_of_valid_measurements > 0);
}
}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
