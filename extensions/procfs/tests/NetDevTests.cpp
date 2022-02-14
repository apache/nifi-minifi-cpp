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
#include "ProcFs.h"

using org::apache::nifi::minifi::extensions::procfs::ProcFs;

TEST_CASE("ProcFSTest NetDev with mock", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs_t0("./mockprocfs_t0");
  auto net_devs_t0 = proc_fs_t0.getNetDevs();
  REQUIRE(net_devs_t0.size() == 3);

  ProcFs proc_fs_t1("./mockprocfs_t1");
  auto net_devs_t1 = proc_fs_t1.getNetDevs();
  REQUIRE(net_devs_t1.size() == 3);

  for (auto& [net_dev_name_t0, net_dev_t0] : net_devs_t0) {
    REQUIRE(net_devs_t1.contains(net_dev_name_t0));
    auto& net_dev_t1 = net_devs_t1.at(net_dev_name_t0);
    REQUIRE(net_dev_t1 >= net_dev_t0);
  }
}

TEST_CASE("ProcFSTest NetDev", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs;
  auto net_devs_t0 = proc_fs.getNetDevs();
  sleep(1);
  auto net_devs_t1 = proc_fs.getNetDevs();

  REQUIRE(net_devs_t0.size() == net_devs_t1.size());

  for (auto& [net_dev_name_t0, net_dev_t0] : net_devs_t0) {
    REQUIRE(net_devs_t1.contains(net_dev_name_t0));
    auto& net_dev_t1 = net_devs_t1.at(net_dev_name_t0);
    REQUIRE(net_dev_t1 >= net_dev_t0);
  }
}

