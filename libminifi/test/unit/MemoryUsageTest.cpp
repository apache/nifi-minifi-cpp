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


#include "utils/OsUtils.h"
#include "../TestBase.h"

TEST_CASE("Test Physical memory usage", "[testphysicalmemoryusage]") {
  constexpr bool cout_enabled = true;
  std::vector<char> v(30000000);
  const auto ram_usage_by_process = utils::OsUtils::getCurrentProcessPhysicalMemoryUsage();
  const auto ram_usage_by_system = utils::OsUtils::getSystemPhysicalMemoryUsage();
  const auto ram_total = utils::OsUtils::getSystemTotalPhysicalMemory();

  if (cout_enabled) {
    std::cout << "Physical Memory used by this process: " << ram_usage_by_process << " bytes" << std::endl;
    std::cout << "Physical Memory used by the system: " << ram_usage_by_system << " bytes" << std::endl;
    std::cout << "Total Physical Memory in the system: " << ram_total << " bytes" << std::endl;
  }
  REQUIRE(ram_usage_by_process >= v.size());
  REQUIRE(v.size()*2 >= ram_usage_by_process);
  REQUIRE(ram_usage_by_system >= ram_usage_by_process);
  REQUIRE(ram_total >= ram_usage_by_system);
}

#ifndef WIN32
size_t GetTotalMemoryLegacy() {
  return static_cast<size_t>(sysconf(_SC_PHYS_PAGES)) * static_cast<size_t>(sysconf(_SC_PAGESIZE));
}

TEST_CASE("Test new and legacy total system memory query equivalency") {
  REQUIRE(GetTotalMemoryLegacy() == utils::OsUtils::getSystemTotalPhysicalMemory());
}
#endif


#ifdef WIN32
TEST_CASE("Test Paging file size", "[testpagingfile]") {
  constexpr bool cout_enabled = true;
  const auto total_paging_file_size = utils::OsUtils::getTotalPagingFileSize();

  if (cout_enabled) {
    std::cout << "Total Paging file size: " << total_paging_file_size << " bytes" << std::endl;
  }
  REQUIRE(total_paging_file_size > 0);
}
#endif
