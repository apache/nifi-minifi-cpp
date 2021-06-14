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

void busySleep(int duration_ms, std::chrono::milliseconds& start_ms, std::chrono::milliseconds& end_ms, const std::chrono::system_clock::time_point& origin) {
  start_ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - origin);
  end_ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - origin);
  while (end_ms-start_ms < std::chrono::milliseconds(duration_ms)) {
    end_ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - origin);
  }
}

void idleSleep(int duration_ms, std::chrono::milliseconds& start_ms, std::chrono::milliseconds& end_ms, const std::chrono::system_clock::time_point& origin) {
  start_ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - origin);
  std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
  end_ms = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now() - origin);
}

void printCpuUtilization(const std::string& target, const std::string& sleep_type, uint64_t start, uint64_t end, double utilizationPercent) {
  std::cout << target << " CPU Utilization during "<< sleep_type << " between " << start << "ms and " << end << "ms : " << utilizationPercent << std::endl;
}


TEST_CASE("Test System CPU Utilization", "[testcpuusage]") {
  constexpr int number_of_rounds = 3;
  constexpr int sleep_duration_ms = 1000;
  constexpr bool cout_enabled = true;

  org::apache::nifi::minifi::utils::SystemCpuUsageTracker hostTracker;
  org::apache::nifi::minifi::utils::ProcessCpuUsageTracker processTracker;
  auto vCores = (std::max)(uint32_t{1}, std::thread::hardware_concurrency());
  auto test_start = std::chrono::system_clock::now();
  for (int i = 0; i < number_of_rounds; ++i) {
    {
      std::chrono::milliseconds idle_sleep_start;
      std::chrono::milliseconds idle_sleep_end;
      idleSleep(sleep_duration_ms, idle_sleep_start, idle_sleep_end, test_start);

      double system_cpu_usage_during_idle_sleep = hostTracker.getCpuUsageAndRestartCollection();
      double process_cpu_usage_during_idle_sleep = processTracker.getCpuUsageAndRestartCollection();
      REQUIRE(system_cpu_usage_during_idle_sleep >= 0);
      REQUIRE(process_cpu_usage_during_idle_sleep >= 0);
      REQUIRE(system_cpu_usage_during_idle_sleep <= 1);
      REQUIRE(process_cpu_usage_during_idle_sleep < 0.1);
      if (cout_enabled) {
        printCpuUtilization("System", "idle sleep", idle_sleep_start.count(), idle_sleep_end.count(), system_cpu_usage_during_idle_sleep);
        printCpuUtilization("Process", "idle sleep", idle_sleep_start.count(), idle_sleep_end.count(), process_cpu_usage_during_idle_sleep);
        std::cout << std::endl;
      }
    }
    {
      std::chrono::milliseconds busy_sleep_start;
      std::chrono::milliseconds busy_sleep_end;
      busySleep(sleep_duration_ms, busy_sleep_start, busy_sleep_end, test_start);

      double system_cpu_usage_during_busy_sleep = hostTracker.getCpuUsageAndRestartCollection();
      double process_cpu_usage_during_busy_sleep = processTracker.getCpuUsageAndRestartCollection();
      REQUIRE(system_cpu_usage_during_busy_sleep > (0.8 / vCores));
      REQUIRE(system_cpu_usage_during_busy_sleep <= 1);
      REQUIRE(process_cpu_usage_during_busy_sleep >= 0);
      REQUIRE(process_cpu_usage_during_busy_sleep <= 1);
      if (cout_enabled) {
        printCpuUtilization("System", "busy sleep", busy_sleep_start.count(), busy_sleep_end.count(), system_cpu_usage_during_busy_sleep);
        printCpuUtilization("Process", "busy sleep", busy_sleep_start.count(), busy_sleep_end.count(), process_cpu_usage_during_busy_sleep);
        std::cout << std::endl;
      }
    }
  }
}
