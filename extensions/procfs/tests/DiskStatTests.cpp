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

using org::apache::nifi::minifi::extensions::procfs::ProcFs;

TEST_CASE("ProcFSTest DiskStat with mock", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs_t0("./mockprocfs_t0");
  auto disk_stats_t0 = proc_fs_t0.getDiskStats();
  REQUIRE(disk_stats_t0.size() == 17);

  REQUIRE(disk_stats_t0.contains("nvme0n1p3"));
  CHECK(disk_stats_t0.at("nvme0n1p3").getMajorDeviceNumber() == 259);
  CHECK(disk_stats_t0.at("nvme0n1p3").getMinorDeviceNumber() == 3);
  CHECK(disk_stats_t0.at("nvme0n1p3").getReadsCompleted() == 59);
  CHECK(disk_stats_t0.at("nvme0n1p3").getReadsMerged() == 1);
  CHECK(disk_stats_t0.at("nvme0n1p3").getSectorsRead() == 4526);
  CHECK(disk_stats_t0.at("nvme0n1p3").getMillisecondsSpentReading() == 30);
  CHECK(disk_stats_t0.at("nvme0n1p3").getWritesCompleted() == 510);
  CHECK(disk_stats_t0.at("nvme0n1p3").getWritesMerged() == 3533);
  CHECK(disk_stats_t0.at("nvme0n1p3").getSectorsWritten() == 32344);
  CHECK(disk_stats_t0.at("nvme0n1p3").getMillisecondsSpentWriting() == 15021);
  CHECK(disk_stats_t0.at("nvme0n1p3").getIosInProgress() == 0);
  CHECK(disk_stats_t0.at("nvme0n1p3").getMillisecondsSpentIo() == 6150);
  CHECK(disk_stats_t0.at("nvme0n1p3").getWeightedMillisecondsSpentIo() == 15052);


  ProcFs proc_fs_t1("./mockprocfs_t1");
  auto disk_stats_t1 = proc_fs_t1.getDiskStats();
  REQUIRE(disk_stats_t1.size() == 17);

  for (const auto& [disk_name_t0, disk_stat_t0] : disk_stats_t0) {
    REQUIRE(disk_stats_t1.contains(disk_name_t0));
    const auto& disk_stat_t1 = disk_stats_t1.at(disk_name_t0);
    REQUIRE(disk_stat_t0.getMinorDeviceNumber() == disk_stat_t1.getMinorDeviceNumber());
    REQUIRE(disk_stat_t0.getMajorDeviceNumber() == disk_stat_t1.getMajorDeviceNumber());
    REQUIRE(disk_stat_t1.getMonotonicIncreasingMembers() >= disk_stat_t0.getMonotonicIncreasingMembers());
  }
}

TEST_CASE("ProcFSTest DiskStat", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs;
  auto disk_stats_t0 = proc_fs.getDiskStats();
  sleep(1);
  auto disk_stats_t1 = proc_fs.getDiskStats();

  REQUIRE(disk_stats_t1.size() == disk_stats_t0.size());

  for (const auto& [disk_name_t0, disk_stat_t0] : disk_stats_t0) {
    REQUIRE(disk_stats_t1.contains(disk_name_t0));
    const auto& disk_stat_t1 = disk_stats_t1.at(disk_name_t0);
    REQUIRE(disk_stat_t0.getMinorDeviceNumber() == disk_stat_t1.getMinorDeviceNumber());
    REQUIRE(disk_stat_t0.getMajorDeviceNumber() == disk_stat_t1.getMajorDeviceNumber());
    REQUIRE(disk_stat_t1.getMonotonicIncreasingMembers() >= disk_stat_t0.getMonotonicIncreasingMembers());
  }
}
