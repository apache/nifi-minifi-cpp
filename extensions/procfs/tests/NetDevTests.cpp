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

  REQUIRE(net_devs_t0.contains("enp30s0"));
  CHECK(net_devs_t0.at("enp30s0").getBytesReceived() == 46693578527);
  CHECK(net_devs_t0.at("enp30s0").getPacketsReceived() == 52256526);
  CHECK(net_devs_t0.at("enp30s0").getReceiveErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getReceiveDropErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getReceiveFifoErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getReceiveFrameErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getCompressedPacketsReceived() == 0);
  CHECK(net_devs_t0.at("enp30s0").getMulticastFramesReceived() == 4550);
  CHECK(net_devs_t0.at("enp30s0").getBytesTransmitted() == 53470695397);
  CHECK(net_devs_t0.at("enp30s0").getPacketsTransmitted() == 56707053);
  CHECK(net_devs_t0.at("enp30s0").getTransmitErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getTransmitDropErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getTransmitFifoErrors() == 0);
  CHECK(net_devs_t0.at("enp30s0").getTransmitCollisions() == 0);
  CHECK(net_devs_t0.at("enp30s0").getTransmitCarrierLosses() == 0);
  CHECK(net_devs_t0.at("enp30s0").getCompressedPacketsTransmitted() == 0);

  ProcFs proc_fs_t1("./mockprocfs_t1");
  auto net_devs_t1 = proc_fs_t1.getNetDevs();
  REQUIRE(net_devs_t1.size() == 3);

  for (const auto& [net_dev_name_t0, net_dev_t0] : net_devs_t0) {
    REQUIRE(net_devs_t1.contains(net_dev_name_t0));
    const auto& net_dev_t1 = net_devs_t1.at(net_dev_name_t0);
    REQUIRE(net_dev_t1 >= net_dev_t0);
  }
}

TEST_CASE("ProcFSTest NetDev", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs;
  auto net_devs_t0 = proc_fs.getNetDevs();
  sleep(1);
  auto net_devs_t1 = proc_fs.getNetDevs();

  REQUIRE(net_devs_t0.size() == net_devs_t1.size());

  for (const auto& [net_dev_name_t0, net_dev_t0] : net_devs_t0) {
    REQUIRE(net_devs_t1.contains(net_dev_name_t0));
    const auto& net_dev_t1 = net_devs_t1.at(net_dev_name_t0);
    REQUIRE(net_dev_t1 >= net_dev_t0);
  }
}

