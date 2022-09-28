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

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>
#include "core/logging/Logger.h"

#ifdef WIN32
struct _IP_ADAPTER_ADDRESSES_LH;
typedef _IP_ADAPTER_ADDRESSES_LH IP_ADAPTER_ADDRESSES_LH;
typedef IP_ADAPTER_ADDRESSES_LH IP_ADAPTER_ADDRESSES;
#else
struct ifaddrs;
#endif

namespace org::apache::nifi::minifi::utils {
class NetworkInterfaceInfo {
 public:
  NetworkInterfaceInfo(NetworkInterfaceInfo&& src) noexcept = default;
#ifdef WIN32
  // Creates NetworkInterfaceInfo from IP_ADAPTER_ADDRESSES struct (it should contain all ip addresses from an adapter)
  explicit NetworkInterfaceInfo(const IP_ADAPTER_ADDRESSES* adapter);
#else
  // Creates NetworkInterfaceInfo from ifaddrs struct (it will only contain a single ip address from an adapter, it should be merged together based on name_)
  explicit NetworkInterfaceInfo(const struct ifaddrs* ifa);
#endif
  NetworkInterfaceInfo& operator=(NetworkInterfaceInfo&& other) noexcept = default;
  [[nodiscard]] const std::string& getName() const noexcept { return name_; }
  [[nodiscard]] bool hasIpV4Address() const noexcept { return !ip_v4_addresses_.empty(); }
  [[nodiscard]] bool hasIpV6Address() const noexcept { return !ip_v6_addresses_.empty(); }
  [[nodiscard]] bool isRunning() const noexcept { return running_; }
  [[nodiscard]] bool isLoopback() const noexcept { return loopback_; }
  [[nodiscard]] const std::vector<std::string>& getIpV4Addresses() const noexcept { return ip_v4_addresses_; }
  [[nodiscard]] const std::vector<std::string>& getIpV6Addresses() const noexcept { return ip_v6_addresses_; }

  // Traverses the ip addresses and merges them together based on the interface name
  static std::vector<NetworkInterfaceInfo> getNetworkInterfaceInfos(
      const std::function<bool(const NetworkInterfaceInfo&)>& filter = { [](const NetworkInterfaceInfo&) { return true; } },
      std::optional<uint32_t> max_interfaces = std::nullopt);

 private:
  void moveAddressesInto(NetworkInterfaceInfo& destination);

  std::string name_;
  std::vector<std::string> ip_v4_addresses_;
  std::vector<std::string> ip_v6_addresses_;
  bool running_;
  bool loopback_;
  static std::shared_ptr<core::logging::Logger> logger_;
};
}  // namespace org::apache::nifi::minifi::utils
