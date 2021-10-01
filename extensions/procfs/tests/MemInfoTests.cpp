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
using org::apache::nifi::minifi::procfs::MemInfo;

void check_mem_info_json(const rapidjson::Document& document, const MemInfo& mem_info) {
  REQUIRE(document["MemTotal"].GetUint64() == mem_info.getTotalMemory());
  REQUIRE(document["MemFree"].GetUint64() == mem_info.getFreeMemory());
  REQUIRE(document["MemAvailable"].GetUint64() == mem_info.getAvailableMemory());
  REQUIRE(document["SwapTotal"].GetUint64() == mem_info.getTotalSwap());
  REQUIRE(document["SwapFree"].GetUint64() == mem_info.getFreeSwap());
}

void test_mem_info(const ProcFs& proc_fs) {
  auto mem_info = proc_fs.getMemInfo();
  REQUIRE(mem_info.has_value());
  rapidjson::Document document(rapidjson::kObjectType);
  mem_info.value().addToJson(document, document.GetAllocator());
  check_mem_info_json(document, mem_info.value());
}

TEST_CASE("ProcFSTest meminfo test with mock", "[procfmeminfomocktest]") {
  ProcFs proc_fs("./mockprocfs_t0");
  test_mem_info(proc_fs);
}

TEST_CASE("ProcFSTest meminfo test", "[procfsmeminfotest]") {
  ProcFs proc_fs;
  test_mem_info(proc_fs);
}
