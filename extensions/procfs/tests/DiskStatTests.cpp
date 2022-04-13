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
#include "DiskStat.h"
#include "MockProcFs.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

TEST_CASE("ProcFSTest DiskStat with mock", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs_t0 = mock_proc_fs_t0();
  auto disk_stats_t0 = proc_fs_t0.getDiskStats();
  REQUIRE(disk_stats_t0.size() == 17);

  REQUIRE(disk_stats_t0[3].first == "nvme0n1p3");
  CHECK(disk_stats_t0[3].second.getMajorDeviceNumber() == 259);
  CHECK(disk_stats_t0[3].second.getMinorDeviceNumber() == 3);
  CHECK(disk_stats_t0[3].second.getReadsCompleted() == 59);
  CHECK(disk_stats_t0[3].second.getReadsMerged() == 1);
  CHECK(disk_stats_t0[3].second.getSectorsRead() == 4526);
  CHECK(disk_stats_t0[3].second.getMillisecondsSpentReading() == 30);
  CHECK(disk_stats_t0[3].second.getWritesCompleted() == 510);
  CHECK(disk_stats_t0[3].second.getWritesMerged() == 3533);
  CHECK(disk_stats_t0[3].second.getSectorsWritten() == 32344);
  CHECK(disk_stats_t0[3].second.getMillisecondsSpentWriting() == 15021);
  CHECK(disk_stats_t0[3].second.getIosInProgress() == 0);
  CHECK(disk_stats_t0[3].second.getMillisecondsSpentIo() == 6150);
  CHECK(disk_stats_t0[3].second.getWeightedMillisecondsSpentIo() == 15052);


  ProcFs proc_fs_t1 = mock_proc_fs_t1();
  auto disk_stats_t1 = proc_fs_t1.getDiskStats();
  REQUIRE(disk_stats_t1.size() == 17);

  for (const auto& disk_stat_t0: disk_stats_t0) {
    auto disk_stat_t1 = std::find_if(disk_stats_t1.begin(), disk_stats_t1.end(), [&disk_stat_t0](auto& disk_stat_t1) { return disk_stat_t1.first == disk_stat_t0.first; });
    REQUIRE(disk_stat_t1 != disk_stats_t1.end());
    REQUIRE(disk_stat_t0.second.getMinorDeviceNumber() == disk_stat_t1->second.getMinorDeviceNumber());
    REQUIRE(disk_stat_t0.second.getMajorDeviceNumber() == disk_stat_t1->second.getMajorDeviceNumber());
    REQUIRE(disk_stat_t1->second.getMonotonicIncreasingMembers() >= disk_stat_t0.second.getMonotonicIncreasingMembers());
  }
}

TEST_CASE("ProcFSTest DiskStat", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs;
  auto disk_stats_t0 = proc_fs.getDiskStats();
  std::this_thread::sleep_for(100ms);
  auto disk_stats_t1 = proc_fs.getDiskStats();

  REQUIRE(disk_stats_t1.size() == disk_stats_t0.size());

  for (const auto& disk_stat_t0: disk_stats_t0) {
    auto disk_stat_t1 = std::find_if(disk_stats_t1.begin(), disk_stats_t1.end(), [&disk_stat_t0](auto& disk_stat_t1) { return disk_stat_t1.first == disk_stat_t0.first; });
    REQUIRE(disk_stat_t1 != disk_stats_t1.end());
    REQUIRE(disk_stat_t0.second.getMinorDeviceNumber() == disk_stat_t1->second.getMinorDeviceNumber());
    REQUIRE(disk_stat_t0.second.getMajorDeviceNumber() == disk_stat_t1->second.getMajorDeviceNumber());
    REQUIRE(disk_stat_t1->second.getMonotonicIncreasingMembers() >= disk_stat_t0.second.getMonotonicIncreasingMembers());
  }
}
}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
