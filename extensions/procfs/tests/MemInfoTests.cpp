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

#include "unit/Catch.h"
#include "ProcFs.h"
#include "utils/Literals.h"
#include "MockProcFs.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

TEST_CASE("ProcFSTest meminfo test with mock", "[procfmeminfomocktest]") {
  ProcFs proc_fs(get_procfs_test_dir() / "mockprocfs_t0");
  auto mem_info = proc_fs.getMemInfo();
  REQUIRE(mem_info);
  CHECK(mem_info->getTotalMemory() == 32895000_KiB);
  CHECK(mem_info->getFreeMemory() == 9870588_KiB);
  CHECK(mem_info->getAvailableMemory() == 22167288_KiB);
  CHECK(mem_info->getTotalSwap() == 9227464_KiB);
  CHECK(mem_info->getFreeSwap() == 9188320_KiB);
}

TEST_CASE("ProcFSTest meminfo test", "[procfsmeminfotest]") {
  ProcFs proc_fs;
  auto mem_info = proc_fs.getMemInfo();
  REQUIRE(mem_info);
}
}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
