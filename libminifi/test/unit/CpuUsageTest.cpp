/**
 *
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


#include <chrono>
#include <thread>
#include "utils/SystemCpuUsageTracker.h"
#include "utils/ProcessCpuUsageTracker.h"
#include "../TestBase.h"
#include "../Catch.h"

using namespace std::literals::chrono_literals;
using steady_clock = std::chrono::steady_clock;
using milliseconds = std::chrono::milliseconds;

steady_clock::duration busySleep(const milliseconds duration) {
  auto start_time = steady_clock::now();
  while (steady_clock::now()-start_time < duration) {
    // noop
  }
  return steady_clock::now() - start_time;
}

steady_clock::duration idleSleep(const milliseconds duration) {
  auto start_time = steady_clock::now();
  std::this_thread::sleep_for(duration);
  return steady_clock::now() - start_time;
}

void printCpuUtilization(const std::string& target, const std::string& sleep_type, const steady_clock::duration& sleep_duration, double utilizationPercent) {
  std::cout << target << " CPU Utilization during "<< sleep_type << " lasting " << duration_cast<milliseconds>(sleep_duration).count() << "ms : " << utilizationPercent << std::endl;
}


TEST_CASE("Test System CPU Utilization", "[testcpuusage]") {
  constexpr int number_of_rounds = 3;
  constexpr milliseconds sleep_duration = 1s;
  constexpr bool cout_enabled = true;

  org::apache::nifi::minifi::utils::SystemCpuUsageTracker hostTracker;
  org::apache::nifi::minifi::utils::ProcessCpuUsageTracker processTracker;
  auto vCores = (std::max)(uint32_t{1}, std::thread::hardware_concurrency());
  for (int i = 0; i < number_of_rounds; ++i) {
    {
      auto idle_sleep_duration = idleSleep(sleep_duration);

      double system_cpu_usage_during_idle_sleep = hostTracker.getCpuUsageAndRestartCollection();
      double process_cpu_usage_during_idle_sleep = processTracker.getCpuUsageAndRestartCollection();
      REQUIRE(system_cpu_usage_during_idle_sleep >= 0);
      REQUIRE(process_cpu_usage_during_idle_sleep >= 0);
      REQUIRE(system_cpu_usage_during_idle_sleep <= 1);
      REQUIRE(process_cpu_usage_during_idle_sleep < 0.1);
      if (cout_enabled) {
        printCpuUtilization("System", "idle sleep", idle_sleep_duration, system_cpu_usage_during_idle_sleep);
        printCpuUtilization("Process", "idle sleep", idle_sleep_duration, process_cpu_usage_during_idle_sleep);
        std::cout << std::endl;
      }
    }
    {
      auto busy_sleep_duration = busySleep(sleep_duration);

      double system_cpu_usage_during_busy_sleep = hostTracker.getCpuUsageAndRestartCollection();
      double process_cpu_usage_during_busy_sleep = processTracker.getCpuUsageAndRestartCollection();
      REQUIRE(system_cpu_usage_during_busy_sleep > (0.8 / vCores));
      REQUIRE(system_cpu_usage_during_busy_sleep <= 1);
      REQUIRE(process_cpu_usage_during_busy_sleep >= 0);
      REQUIRE(process_cpu_usage_during_busy_sleep <= 1);
      if (cout_enabled) {
        printCpuUtilization("System", "busy sleep", busy_sleep_duration, system_cpu_usage_during_busy_sleep);
        printCpuUtilization("Process", "busy sleep", busy_sleep_duration, process_cpu_usage_during_busy_sleep);
        std::cout << std::endl;
      }
    }
  }
}
