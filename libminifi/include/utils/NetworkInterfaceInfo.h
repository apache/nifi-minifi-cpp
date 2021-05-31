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
#include <unordered_map>
#include "utils/OptionalUtils.h"

#ifdef WIN32
struct _IP_ADAPTER_ADDRESSES_LH;
typedef _IP_ADAPTER_ADDRESSES_LH IP_ADAPTER_ADDRESSES_LH;
typedef IP_ADAPTER_ADDRESSES_LH IP_ADAPTER_ADDRESSES;
#else
struct ifaddrs;
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
class NetworkInterfaceInfo {
 public:
  NetworkInterfaceInfo(NetworkInterfaceInfo&& src) = default;
#ifdef WIN32
  explicit NetworkInterfaceInfo(const IP_ADAPTER_ADDRESSES* adapter);
#else
  explicit NetworkInterfaceInfo(const struct ifaddrs* ifa);
#endif
  const std::string& getName() const { return name_; }
  bool hasIpV4Address() const { return ip_v4_addresses_.size() > 0; }
  bool hasIpV6Address() const { return ip_v6_addresses_.size() > 0; }
  bool isRunning() const { return running_; }
  bool isLoopback() const { return loopback_; }
  const std::vector<std::string>& getIpV4Addresses() const { return ip_v4_addresses_; }
  const std::vector<std::string>& getIpV6Addresses() const { return ip_v6_addresses_; }

  void moveAddressesInto(NetworkInterfaceInfo& destination);

  static std::unordered_map<std::string, NetworkInterfaceInfo> getNetworkInterfaceInfos(std::function<bool(const NetworkInterfaceInfo&)> filter = { [](const NetworkInterfaceInfo&) { return true; } },
                                                                                        const utils::optional<uint32_t> max_interfaces = utils::nullopt);

 private:
  std::string name_;
  std::vector<std::string> ip_v4_addresses_;
  std::vector<std::string> ip_v6_addresses_;
  bool running_;
  bool loopback_;
};
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
