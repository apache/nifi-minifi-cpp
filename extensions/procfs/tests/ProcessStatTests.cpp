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

using org::apache::nifi::minifi::procfs::ProcFs;
using org::apache::nifi::minifi::procfs::ProcessStat;
using org::apache::nifi::minifi::procfs::ProcessStatPeriod;

TEST_CASE("ProcFSTest process test with mock", "[procfsprocessmocktest]") {
  ProcFs proc_fs("./mockprocfs_t0");
  auto process_stats = proc_fs.getProcessStats();
  REQUIRE(process_stats.size() == 1);
  for (auto& process : process_stats) {
    rapidjson::Document document(rapidjson::kObjectType);
    process.second.addToJson(document, document.GetAllocator());
  }
}

TEST_CASE("ProcFSTest process test", "[procfsprocesstest]") {
  ProcFs proc_fs;
  auto process_stats = proc_fs.getProcessStats();
  for (auto& process : process_stats) {
    rapidjson::Document document(rapidjson::kObjectType);
    process.second.addToJson(document, document.GetAllocator());
  }
}

TEST_CASE("ProcFSTest process period test", "[procfsnetdevperiodtest]") {
  ProcFs proc_fs;
  auto process_stats_t0 = proc_fs.getProcessStats();
  auto cpu_stats_t0 = proc_fs.getCpuStats();
  sleep(1);
  auto process_stats_t1 = proc_fs.getProcessStats();
  auto cpu_stats_t1 = proc_fs.getCpuStats();
  double last_cpu_period = cpu_stats_t1[0].getData().getTotal()-cpu_stats_t0[0].getData().getTotal();
  for (const auto& process_stat : process_stats_t1) {
      auto last_stat_it = process_stats_t0.find(process_stat.first);
      if (last_stat_it != process_stats_t0.end()) {
        auto process_stat_period = ProcessStatPeriod::create(last_stat_it->second, process_stat.second, last_cpu_period);
        if (process_stat_period.has_value()) {
          rapidjson::Document document(rapidjson::kObjectType);
          REQUIRE_NOTHROW(process_stat_period->addToJson(document, document.GetAllocator()));
        }
      }
  }
}

