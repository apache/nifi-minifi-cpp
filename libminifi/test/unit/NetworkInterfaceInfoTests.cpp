// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/net/NetworkInterfaceInfo.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("NetworkInterfaceInfo test", "[testnetworkinterfaceinfo]") {
  auto network_interface_infos = utils::NetworkInterfaceInfo::getNetworkInterfaceInfos();
  REQUIRE_FALSE(network_interface_infos.empty());

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
