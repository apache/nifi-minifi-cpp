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

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/NetworkInterfaceInfo.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("NetworkInterfaceInfo test", "[testnetworkinterfaceinfo]") {
  auto network_interface_infos = utils::NetworkInterfaceInfo::getNetworkInterfaceInfos();
  REQUIRE(network_interface_infos.size() > 0);

  auto valid_interface_name = network_interface_infos.begin()->getName();
  auto filter = [&valid_interface_name](const utils::NetworkInterfaceInfo& interface_info) -> bool { return interface_info.getName() == valid_interface_name; };
  REQUIRE(utils::NetworkInterfaceInfo::getNetworkInterfaceInfos(filter, 1).size() == 1);
}

bool is_localhost(const std::string& ip_address) {
  return ip_address == "127.0.0.1";
}

TEST_CASE("NetworkInterfaceInfo isLoopback test", "[testnetworkinterfaceinfoloopback]") {
  auto network_interface_infos = utils::NetworkInterfaceInfo::getNetworkInterfaceInfos([] (const utils::NetworkInterfaceInfo& interface_info) -> bool { return !interface_info.isLoopback();});
  for (auto& network_interface_info : network_interface_infos) {
    auto& ip_v4_addresses = network_interface_info.getIpV4Addresses();
    REQUIRE(std::none_of(ip_v4_addresses.begin(), ip_v4_addresses.end(), is_localhost));
  }
}
