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

#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

using org::apache::nifi::minifi::procfs::ProcFs;
using org::apache::nifi::minifi::procfs::CpuStat;
using org::apache::nifi::minifi::procfs::CpuStatPeriod;


void check_cpu_stat_json(const rapidjson::Value& root, const CpuStat& cpu_stat) {
  REQUIRE(root.HasMember(cpu_stat.getName().c_str()));
  const rapidjson::Value& cpu_stat_json = root[cpu_stat.getName().c_str()];
  REQUIRE(cpu_stat_json[CpuStat::USER_STR].GetUint64() == cpu_stat.getData().getUser());
  REQUIRE(cpu_stat_json[CpuStat::NICE_STR].GetUint64() == cpu_stat.getData().getNice());
  REQUIRE(cpu_stat_json[CpuStat::SYSTEM_STR].GetUint64() == cpu_stat.getData().getSystem());
  REQUIRE(cpu_stat_json[CpuStat::IDLE_STR].GetUint64() == cpu_stat.getData().getIdle());
  REQUIRE(cpu_stat_json[CpuStat::IO_WAIT_STR].GetUint64() == cpu_stat.getData().getIoWait());
  REQUIRE(cpu_stat_json[CpuStat::IRQ_STR].GetUint64() == cpu_stat.getData().getIrq());
  REQUIRE(cpu_stat_json[CpuStat::SOFT_IRQ_STR].GetUint64() == cpu_stat.getData().getSoftIrq());
  REQUIRE(cpu_stat_json[CpuStat::STEAL_STR].GetUint64() == cpu_stat.getData().getSteal());
  REQUIRE(cpu_stat_json[CpuStat::GUEST_STR].GetUint64() == cpu_stat.getData().getGuest());
  REQUIRE(cpu_stat_json[CpuStat::GUEST_NICE_STR].GetUint64() == cpu_stat.getData().getGuestNice());
}

void test_cpu_stats(const std::vector<CpuStat>& cpu_stats) {
  for (const auto& cpu_stat : cpu_stats) {
    rapidjson::Document document(rapidjson::kObjectType);
    REQUIRE_NOTHROW(cpu_stat.addToJson(document, document.GetAllocator()));
    check_cpu_stat_json(document, cpu_stat);
  }
}

TEST_CASE("ProcFSTest stat test with mock", "[procfsstatmockabsolutetest]") {
  ProcFs proc_fs("./mockprocfs_t0");
  std::vector<CpuStat> cpu_stats = proc_fs.getCpuStats();
  REQUIRE(cpu_stats.size() == 13);
  test_cpu_stats(cpu_stats);
}

TEST_CASE("ProcFSTest stat test", "[procfsstatabsolutetest]") {
  ProcFs proc_fs;
  std::vector<CpuStat> cpu_stats = proc_fs.getCpuStats();
  REQUIRE(cpu_stats.size());
  test_cpu_stats(cpu_stats);
}

void cpu_stat_period_total_should_be_one(const CpuStatPeriod& cpu_stat) {
  double percentage = 0;
  percentage += cpu_stat.getUserPercent();
  percentage += cpu_stat.getNicePercent();
  percentage += cpu_stat.getSystemPercent();
  percentage += cpu_stat.getIdlePercent();
  percentage += cpu_stat.getIoWaitPercent();
  percentage += cpu_stat.getIrqPercent();
  percentage += cpu_stat.getSoftIrqPercent();
  percentage += cpu_stat.getStealPercent();
  percentage += cpu_stat.getGuestPercent();
  percentage += cpu_stat.getGuestNicePercent();
  REQUIRE(percentage == Approx(1.0));
}

void check_cpu_stat_period_json(const rapidjson::Value& root, const CpuStatPeriod& cpu_stat) {
  REQUIRE(root.HasMember(cpu_stat.getName().c_str()));
  const rapidjson::Value& cpu_stat_json = root[cpu_stat.getName().c_str()];
  REQUIRE(cpu_stat_json[CpuStatPeriod::USER_PERCENT_STR].GetDouble() == cpu_stat.getUserPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::NICE_PERCENT_STR].GetDouble() == cpu_stat.getNicePercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::SYSTEM_PERCENT_STR].GetDouble() == cpu_stat.getSystemPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::IDLE_PERCENT_STR].GetDouble() == cpu_stat.getIdlePercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::IO_WAIT_PERCENT_STR].GetDouble() == cpu_stat.getIoWaitPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::IRQ_PERCENT_STR].GetDouble() == cpu_stat.getIrqPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::SOFT_IRQ_PERCENT_STR].GetDouble() == cpu_stat.getSoftIrqPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::STEAL_PERCENT_STR].GetDouble() == cpu_stat.getStealPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::GUEST_PERCENT_STR].GetDouble() == cpu_stat.getGuestPercent());
  REQUIRE(cpu_stat_json[CpuStatPeriod::GUEST_NICE_PERCENT_STR].GetDouble() == cpu_stat.getGuestNicePercent());
}

void test_cpu_stat_periods(const std::vector<CpuStat>& cpu_stat_start, const std::vector<CpuStat>& cpu_stat_end) {
  REQUIRE(cpu_stat_start.size() == cpu_stat_end.size());
  REQUIRE(cpu_stat_start.size() > 0);
  for (uint32_t i = 0; i < cpu_stat_start.size(); ++i) {
    rapidjson::Document document(rapidjson::kObjectType);
    auto diff = CpuStatPeriod::create(cpu_stat_start[i], cpu_stat_end[i]);
    REQUIRE(diff.has_value());
    REQUIRE_NOTHROW(diff.value().addToJson(document, document.GetAllocator()));
    check_cpu_stat_period_json(document, diff.value());
    cpu_stat_period_total_should_be_one(diff.value());
  }
}

TEST_CASE("ProcFSTest relative stat test with mock", "[procfsstatmockrelativetest]") {
  ProcFs proc_fs_t0("./mockprocfs_t0");
  std::vector<CpuStat> cpu_stats_t0 = proc_fs_t0.getCpuStats();
  REQUIRE(cpu_stats_t0.size() == 13);

  ProcFs proc_fs_t1("./mockprocfs_t1");
  std::vector<CpuStat> cpu_stats_t1 = proc_fs_t1.getCpuStats();
  REQUIRE(cpu_stats_t1.size() == 13);

  test_cpu_stat_periods(cpu_stats_t0, cpu_stats_t1);
}

TEST_CASE("ProcFSTest relative stat from same data is invalid", "[procfsstatmockrelativeinvalidtest]") {
  ProcFs proc_fs_t0("./mockprocfs_t0");
  std::vector<CpuStat> cpu_stats_t0 = proc_fs_t0.getCpuStats();
  for (uint32_t i = 0; i < cpu_stats_t0.size(); ++i) {
    rapidjson::Document document(rapidjson::kObjectType);
    auto diff = CpuStatPeriod::create(cpu_stats_t0[i], cpu_stats_t0[i]);
    REQUIRE_FALSE(diff.has_value());
  }
}

TEST_CASE("ProcFSTest relative stat test", "[procfsstatrelativetest]") {
  ProcFs proc_fs;
  std::vector<CpuStat> cpu_stats_t0 = proc_fs.getCpuStats();
  sleep(1);
  std::vector<CpuStat> cpu_stats_t1 = proc_fs.getCpuStats();

  test_cpu_stat_periods(cpu_stats_t0, cpu_stats_t1);
}
