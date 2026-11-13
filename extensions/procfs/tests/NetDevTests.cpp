// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/Catch.h"
#include "ProcFs.h"
#include "MockProcFs.h"

namespace org::apache::nifi::minifi::extensions::procfs::tests {

TEST_CASE("ProcFSTest NetDev with mock", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs_t0 = mock_proc_fs_t0();
  auto net_devs_t0 = proc_fs_t0.getNetDevs();
  REQUIRE(net_devs_t0.size() == 3);

  REQUIRE(net_devs_t0[1].first == "enp30s0");
  CHECK(net_devs_t0[1].second.getBytesReceived() == 46693578527);
  CHECK(net_devs_t0[1].second.getPacketsReceived() == 52256526);
  CHECK(net_devs_t0[1].second.getReceiveErrors() == 0);
  CHECK(net_devs_t0[1].second.getReceiveDropErrors() == 0);
  CHECK(net_devs_t0[1].second.getReceiveFifoErrors() == 0);
  CHECK(net_devs_t0[1].second.getReceiveFrameErrors() == 0);
  CHECK(net_devs_t0[1].second.getCompressedPacketsReceived() == 0);
  CHECK(net_devs_t0[1].second.getMulticastFramesReceived() == 4550);
  CHECK(net_devs_t0[1].second.getBytesTransmitted() == 53470695397);
  CHECK(net_devs_t0[1].second.getPacketsTransmitted() == 56707053);
  CHECK(net_devs_t0[1].second.getTransmitErrors() == 0);
  CHECK(net_devs_t0[1].second.getTransmitDropErrors() == 0);
  CHECK(net_devs_t0[1].second.getTransmitFifoErrors() == 0);
  CHECK(net_devs_t0[1].second.getTransmitCollisions() == 0);
  CHECK(net_devs_t0[1].second.getTransmitCarrierLosses() == 0);
  CHECK(net_devs_t0[1].second.getCompressedPacketsTransmitted() == 0);

  ProcFs proc_fs_t1 = mock_proc_fs_t1();
  auto net_devs_t1 = proc_fs_t1.getNetDevs();
  REQUIRE(net_devs_t1.size() == 3);

  for (const auto& net_dev_t0 : net_devs_t0) {
    auto net_dev_t1 = std::find_if(net_devs_t1.begin(), net_devs_t1.end(), [&net_dev_t0](auto& net_dev_t1){return net_dev_t1.first == net_dev_t0.first;});
    REQUIRE(net_dev_t1 != net_devs_t1.end());
    REQUIRE(net_dev_t1->second >= net_dev_t0.second);
  }
}

TEST_CASE("ProcFSTest NetDev", "[procfsdiskstatmocktest]") {
  ProcFs proc_fs;
  auto net_devs_t0 = proc_fs.getNetDevs();
  std::this_thread::sleep_for(100ms);
  auto net_devs_t1 = proc_fs.getNetDevs();

  REQUIRE(net_devs_t0.size() == net_devs_t1.size());

  for (const auto& net_dev_t0 : net_devs_t0) {
    auto net_dev_t1 = std::find_if(net_devs_t1.begin(), net_devs_t1.end(), [&net_dev_t0](auto& net_dev_t1){return net_dev_t1.first == net_dev_t0.first;});
    REQUIRE(net_dev_t1 != net_devs_t1.end());
    REQUIRE(net_dev_t1->second >= net_dev_t0.second);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::procfs::tests
