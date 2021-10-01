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

#include "TestBase.h"
#include "ProcFs.h"
#include "DiskStat.h"

using org::apache::nifi::minifi::procfs::ProcFs;
using org::apache::nifi::minifi::procfs::DiskStat;
using org::apache::nifi::minifi::procfs::DiskStatPeriod;

void test_disk_stats(const ProcFs& proc_fs) {
  auto disk_stats = proc_fs.getDiskStats();
  REQUIRE(disk_stats.size() > 0);
  for (const auto& disk_stat : disk_stats) {
    rapidjson::Document document(rapidjson::kObjectType);
    REQUIRE_NOTHROW(disk_stat.addToJson(document, document.GetAllocator()));
  }
}

TEST_CASE("ProcFSTest disk stat test with mock", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs("./mockprocfs_t0");
  test_disk_stats(proc_fs);
}

TEST_CASE("ProcFSTest disk stat test", "[procfsdiskstattest]") {
  ProcFs proc_fs;
  test_disk_stats(proc_fs);
}

void test_net_dev_period(const std::vector<DiskStat>& disk_stats_start, const std::vector<DiskStat>& disk_stats_end) {
  REQUIRE(disk_stats_start.size() == disk_stats_end.size());
  REQUIRE(disk_stats_start.size() > 0);
  for (uint32_t i = 0; i < disk_stats_start.size(); ++i) {
    rapidjson::Document document(rapidjson::kObjectType);
    auto disk_stat_period = DiskStatPeriod::create(disk_stats_start[i], disk_stats_end[i]);
    REQUIRE(disk_stat_period.has_value());
    REQUIRE_NOTHROW(disk_stat_period.value().addToJson(document, document.GetAllocator()));
  }
}

TEST_CASE("ProcFSTest disk stat period test with mock", "[procfsdiskstatperiodmocktest]") {
  ProcFs proc_fs_t0("./mockprocfs_t0");
  std::vector<DiskStat> disk_stats_t0 = proc_fs_t0.getDiskStats();
  sleep(1);
  ProcFs proc_fs_t1("./mockprocfs_t1");
  std::vector<DiskStat> disk_stats_t1 = proc_fs_t1.getDiskStats();
  test_net_dev_period(disk_stats_t0, disk_stats_t1);
}

TEST_CASE("ProcFSTest disk stat period test", "[procfsdiskstatperiodtest]") {
  ProcFs proc_fs;
  std::vector<DiskStat> disk_stats_t0 = proc_fs.getDiskStats();
  sleep(1);
  std::vector<DiskStat> disk_stats_t1 = proc_fs.getDiskStats();
  test_net_dev_period(disk_stats_t0, disk_stats_t1);
}
