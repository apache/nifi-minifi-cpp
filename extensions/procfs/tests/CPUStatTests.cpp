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
#include "catch2/catch_approx.hpp"
#include "ProcFs.h"
#include "MockProcFs.h"

#include "rapidjson/stream.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

void cpu_stat_period_total_should_be_one(const CpuStatData& cpu_stat) {
  double percentage = 0;
  percentage += cpu_stat.getUser() / cpu_stat.getTotal();
  percentage += cpu_stat.getNice() / cpu_stat.getTotal();
  percentage += cpu_stat.getSystem() / cpu_stat.getTotal();
  percentage += cpu_stat.getIdle() / cpu_stat.getTotal();
  percentage += cpu_stat.getIoWait() / cpu_stat.getTotal();
  percentage += cpu_stat.getIrq() / cpu_stat.getTotal();
  percentage += cpu_stat.getSoftIrq() / cpu_stat.getTotal();
  percentage += cpu_stat.getSteal() / cpu_stat.getTotal();
  percentage += cpu_stat.getGuest() / cpu_stat.getTotal();
  percentage += cpu_stat.getGuestNice() / cpu_stat.getTotal();
  REQUIRE(percentage == Catch::Approx(1.0));
}

TEST_CASE("ProcFSTest stat test with mock", "[procfsstatmockabsolutetest]") {
  ProcFs proc_fs_t0 = mock_proc_fs_t0();
  auto cpu_stats_t0 = proc_fs_t0.getCpuStats();
  REQUIRE(cpu_stats_t0.size() == 13);
  for (const auto& [cpu_name, cpu_stat] : cpu_stats_t0) {
    cpu_stat_period_total_should_be_one(cpu_stat);
  }

  REQUIRE(cpu_stats_t0[4].first == "cpu3");
  CHECK(cpu_stats_t0[4].second.getUser() == SystemClockDuration(1299551));
  CHECK(cpu_stats_t0[4].second.getNice() == SystemClockDuration(732));
  CHECK(cpu_stats_t0[4].second.getSystem() == SystemClockDuration(316295));
  CHECK(cpu_stats_t0[4].second.getIdle() == SystemClockDuration(4498383));
  CHECK(cpu_stats_t0[4].second.getIoWait() == SystemClockDuration(1428));
  CHECK(cpu_stats_t0[4].second.getIrq() == SystemClockDuration(22491));
  CHECK(cpu_stats_t0[4].second.getSoftIrq() == SystemClockDuration(7022));
  CHECK(cpu_stats_t0[4].second.getSteal() == SystemClockDuration(0));
  CHECK(cpu_stats_t0[4].second.getGuest() == SystemClockDuration(0));
  CHECK(cpu_stats_t0[4].second.getGuestNice() == SystemClockDuration(0));

  ProcFs proc_fs_t1 = mock_proc_fs_t1();
  auto cpu_stats_t1 = proc_fs_t1.getCpuStats();
  REQUIRE(cpu_stats_t1.size() == 13);
  for (const auto& [cpu_name, cpu_stat] : cpu_stats_t1) {
    cpu_stat_period_total_should_be_one(cpu_stat);
  }

  for (size_t i = 0; i < cpu_stats_t1.size(); ++i) {
    const auto& [cpu_name_t0, cpu_stat_t0] = cpu_stats_t0[i];
    const auto& [cpu_name_t1, cpu_stat_t1] = cpu_stats_t1[i];
    REQUIRE(cpu_name_t0 == cpu_name_t1);
    REQUIRE(cpu_stat_t1 > cpu_stat_t0);
    auto cpu_stat_diff = cpu_stat_t1 - cpu_stat_t0;
    cpu_stat_period_total_should_be_one(cpu_stat_diff);
  }
}

TEST_CASE("ProcFSTest stat test ", "[procfsstatmockabsolutetest]") {
  ProcFs proc_fs;
  auto cpu_stats_t0 = proc_fs.getCpuStats();
  for (const auto& [cpu_name, cpu_stat] : cpu_stats_t0) {
    cpu_stat_period_total_should_be_one(cpu_stat);
  }
  std::this_thread::sleep_for(100ms);
  auto cpu_stats_t1 = proc_fs.getCpuStats();
  for (const auto& [cpu_name, cpu_stat] : cpu_stats_t1) {
    cpu_stat_period_total_should_be_one(cpu_stat);
  }

  REQUIRE(cpu_stats_t0.size() == cpu_stats_t1.size());

  for (size_t i = 0; i < cpu_stats_t1.size(); ++i) {
    const auto& [cpu_name_t0, cpu_stat_t0] = cpu_stats_t0[i];
    const auto& [cpu_name_t1, cpu_stat_t1] = cpu_stats_t1[i];
    REQUIRE(cpu_name_t0 == cpu_name_t1);
    REQUIRE(cpu_stat_t1 > cpu_stat_t0);
    auto cpu_stat_diff = cpu_stat_t1 - cpu_stat_t0;
    cpu_stat_period_total_should_be_one(cpu_stat_diff);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
