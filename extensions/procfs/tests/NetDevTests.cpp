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
using org::apache::nifi::minifi::procfs::NetDev;
using org::apache::nifi::minifi::procfs::NetDevPeriod;

void check_net_dev_json(const rapidjson::Value& root, const NetDev& net_dev) {
  REQUIRE(root.HasMember(net_dev.getInterfaceName().c_str()));
  const rapidjson::Value& net_dev_json = root[net_dev.getInterfaceName().c_str()];
  REQUIRE(net_dev_json[NetDev::BYTES_RECEIVED_STR].GetUint64() == net_dev.getData().getBytesReceived());
  REQUIRE(net_dev_json[NetDev::PACKETS_RECEIVED_STR].GetUint64() == net_dev.getData().getPacketsReceived());
  REQUIRE(net_dev_json[NetDev::RECEIVE_ERRORS_STR].GetUint64() == net_dev.getData().getReceiveErrors());
  REQUIRE(net_dev_json[NetDev::RECEIVE_DROP_ERRORS_STR].GetUint64() == net_dev.getData().getReceiveDropErrors());
  REQUIRE(net_dev_json[NetDev::RECEIVE_FIFO_ERRORS_STR].GetUint64() == net_dev.getData().getReceiveFifoErrors());
  REQUIRE(net_dev_json[NetDev::RECEIVE_FRAME_ERRORS_STR].GetUint64() == net_dev.getData().getReceiveFrameErrors());
  REQUIRE(net_dev_json[NetDev::COMPRESSED_PACKETS_RECEIVED_STR].GetUint64() == net_dev.getData().getCompressedPacketsReceived());
  REQUIRE(net_dev_json[NetDev::MULTICAST_FRAMES_RECEIVED_STR].GetUint64() == net_dev.getData().getMulticastFramesReceived());

  REQUIRE(net_dev_json[NetDev::BYTES_TRANSMITTED_STR].GetUint64() == net_dev.getData().getBytesTransmitted());
  REQUIRE(net_dev_json[NetDev::PACKETS_TRANSMITTED_STR].GetUint64() == net_dev.getData().getPacketsTransmitted());
  REQUIRE(net_dev_json[NetDev::TRANSMIT_ERRORS_STR].GetUint64() == net_dev.getData().getTransmitErrors());
  REQUIRE(net_dev_json[NetDev::TRANSMIT_DROP_ERRORS_STR].GetUint64() == net_dev.getData().getTransmitDropErrors());
  REQUIRE(net_dev_json[NetDev::TRANSMIT_FIFO_ERRORS_STR].GetUint64() == net_dev.getData().getTransmitFifoErrors());
  REQUIRE(net_dev_json[NetDev::TRANSMIT_COLLISIONS_DETECTED_STR].GetUint64() == net_dev.getData().getTransmitCollisions());
  REQUIRE(net_dev_json[NetDev::TRANSMIT_CARRIER_LOSSES_STR].GetUint64() == net_dev.getData().getTransmitCarrierLosses());
  REQUIRE(net_dev_json[NetDev::COMPRESSED_PACKETS_TRANSMITTED_STR].GetUint64() == net_dev.getData().getCompressedPacketsTransmitted());
}

void test_net_dev(const std::vector<NetDev>& net_devs) {
  REQUIRE(net_devs.size() > 0);
  for (auto& net_dev : net_devs) {
    rapidjson::Document document(rapidjson::kObjectType);
    REQUIRE_NOTHROW(net_dev.addToJson(document, document.GetAllocator()));
    check_net_dev_json(document, net_dev);
  }
}

TEST_CASE("ProcFSTest netdev test with mock", "[procfnetdevmocktest]") {
  ProcFs proc_fs("./mockprocfs_t0");
  std::vector<NetDev> net_devs = proc_fs.getNetDevs();
  REQUIRE(net_devs.size() == 3);
  test_net_dev(net_devs);
}

TEST_CASE("ProcFSTest netdev test", "[procfnetdevtest]") {
  ProcFs proc_fs;
  std::vector<NetDev> net_devs = proc_fs.getNetDevs();
  REQUIRE(net_devs.size() > 0);
  test_net_dev(net_devs);
}

void check_net_dev_period_json(const rapidjson::Value& root, const NetDevPeriod& net_dev_period) {
  REQUIRE(root.HasMember(net_dev_period.getInterfaceName().c_str()));
  const rapidjson::Value& net_dev_json = root[net_dev_period.getInterfaceName().c_str()];
  REQUIRE(net_dev_json[NetDevPeriod::BYTES_RECEIVED_PER_SEC_STR].GetDouble() == net_dev_period.getBytesReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::PACKETS_RECEIVED_PER_SEC_STR].GetDouble() == net_dev_period.getPacketsReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::RECEIVE_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getErrsReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::RECEIVE_DROP_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getDropErrorsReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::RECEIVE_FIFO_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getFifoErrorsReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::RECEIVE_FRAME_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getFrameErrorsReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::COMPRESSED_PACKETS_RECEIVED_PER_SEC_STR].GetDouble() == net_dev_period.getCompressedPacketsReceivedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::MULTICAST_FRAMES_RECEIVED_PER_SEC_STR].GetDouble() == net_dev_period.getMulticastFramesReceivedPerSec());

  REQUIRE(net_dev_json[NetDevPeriod::BYTES_TRANSMITTED_PER_SEC_STR].GetDouble() == net_dev_period.getBytesTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::PACKETS_TRANSMITTED_PER_SEC_STR].GetDouble() == net_dev_period.getPacketsTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::TRANSMIT_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getErrsTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::TRANSMIT_DROP_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getFifoErrorsTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::TRANSMIT_FIFO_ERRORS_PER_SEC_STR].GetDouble() == net_dev_period.getDropErrorsTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::TRANSMIT_COLLISIONS_DETECTED_PER_SEC_STR].GetDouble() == net_dev_period.getCollisionsTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::TRANSMIT_CARRIER_LOSSES_PER_SEC_STR].GetDouble() == net_dev_period.getCarrierLossesTransmittedPerSec());
  REQUIRE(net_dev_json[NetDevPeriod::COMPRESSED_PACKETS_TRANSMITTED_PER_SEC_STR].GetDouble() == net_dev_period.getCompressedPacketsTransmittedPerSec());
}


void test_net_dev_period(const std::vector<NetDev>& net_devs_start, const std::vector<NetDev>& net_devs_end) {
  REQUIRE(net_devs_start.size() == net_devs_end.size());
  REQUIRE(net_devs_start.size() > 0);
  for (uint32_t i = 0; i < net_devs_start.size(); ++i) {
    rapidjson::Document document(rapidjson::kObjectType);
    auto net_dev_period = NetDevPeriod::create(net_devs_start[i], net_devs_end[i]);
    REQUIRE(net_dev_period.has_value());
    REQUIRE_NOTHROW(net_dev_period.value().addToJson(document, document.GetAllocator()));
    check_net_dev_period_json(document, net_dev_period.value());
  }
}

TEST_CASE("ProcFSTest netdev period test with mock", "[procfnetdevperiodmocktest]") {
  ProcFs proc_fs_t0("./mockprocfs_t0");
  std::vector<NetDev> net_devs_t0 = proc_fs_t0.getNetDevs();
  sleep(1);
  ProcFs proc_fs_t1("./mockprocfs_t1");
  std::vector<NetDev> net_devs_t1 = proc_fs_t1.getNetDevs();
  test_net_dev_period(net_devs_t0, net_devs_t1);
}

TEST_CASE("ProcFSTest netdev period test", "[procfsnetdevperiodtest]") {
  ProcFs proc_fs;
  std::vector<NetDev> net_devs_t0 = proc_fs.getNetDevs();
  sleep(1);
  std::vector<NetDev> net_devs_t1 = proc_fs.getNetDevs();
  test_net_dev_period(net_devs_t0, net_devs_t1);
}
