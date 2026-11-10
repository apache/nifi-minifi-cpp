// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/Catch.h"
#include "ProcFs.h"
#include "minifi-cpp/utils/Literals.h"
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
