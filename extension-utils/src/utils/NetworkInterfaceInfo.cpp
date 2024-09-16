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
#include "utils/NetworkInterfaceInfo.h"
#include "utils/net/Socket.h"
#include "core/logging/LoggerFactory.h"
#ifdef WIN32
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#include "utils/OsUtils.h"
#include "utils/UnicodeConversion.h"
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

namespace org::apache::nifi::minifi::utils {

std::shared_ptr<core::logging::Logger> NetworkInterfaceInfo::logger_ = core::logging::LoggerFactory<NetworkInterfaceInfo>::getLogger();

#ifdef WIN32

NetworkInterfaceInfo::NetworkInterfaceInfo(const IP_ADAPTER_ADDRESSES* adapter)
    : name_(to_string(adapter->FriendlyName)),
      running_(adapter->OperStatus == IfOperStatusUp),
      loopback_(adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
  for (auto unicast_address = adapter->FirstUnicastAddress; unicast_address != nullptr; unicast_address = unicast_address->Next) {
    if (unicast_address->Address.lpSockaddr->sa_family == AF_INET) {
      ip_v4_addresses_.push_back(net::sockaddr_ntop(unicast_address->Address.lpSockaddr));
    } else if (unicast_address->Address.lpSockaddr->sa_family == AF_INET6) {
      ip_v6_addresses_.push_back(net::sockaddr_ntop(unicast_address->Address.lpSockaddr));
    }
  }
}
#else
NetworkInterfaceInfo::NetworkInterfaceInfo(const struct ifaddrs* ifa)
    : name_(ifa->ifa_name),
      running_(ifa->ifa_flags & IFF_RUNNING),
      loopback_(ifa->ifa_flags & IFF_LOOPBACK) {
  if (ifa->ifa_addr->sa_family == AF_INET) {
    ip_v4_addresses_.push_back(net::sockaddr_ntop(ifa->ifa_addr));
  } else if (ifa->ifa_addr->sa_family == AF_INET6) {
    ip_v6_addresses_.push_back(net::sockaddr_ntop(ifa->ifa_addr));
  }
}
#endif

namespace {
struct HasName {
  explicit HasName(const std::string& name) : name_(name) {}
  bool operator()(const NetworkInterfaceInfo& network_interface_info) {
    return network_interface_info.getName() == name_;
  }
  const std::string& name_;
};
}

std::vector<NetworkInterfaceInfo> NetworkInterfaceInfo::getNetworkInterfaceInfos(const std::function<bool(const NetworkInterfaceInfo&)>& filter,
    const std::optional<uint32_t> max_interfaces) {
  std::vector<NetworkInterfaceInfo> network_adapters;
#ifdef WIN32
  ULONG buffer_length = sizeof(IP_ADAPTER_ADDRESSES);
  auto get_adapters_err = GetAdaptersAddresses(0, 0, nullptr, nullptr, &buffer_length);
  if (ERROR_BUFFER_OVERFLOW != get_adapters_err) {
    logger_->log_error("GetAdaptersAddresses failed: {}", get_adapters_err);
    return network_adapters;
  }
  std::vector<char> bytes(buffer_length, 0);
  auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(bytes.data());
  get_adapters_err = GetAdaptersAddresses(0, 0, nullptr, adapter, &buffer_length);
  if (NO_ERROR != get_adapters_err) {
    logger_->log_error("GetAdaptersAddresses failed: {}", get_adapters_err);
    return network_adapters;
  }
  while (adapter != nullptr) {
    NetworkInterfaceInfo interface_info(adapter);
    if (filter(interface_info)) {
      auto it = std::find_if(network_adapters.begin(), network_adapters.end(), HasName(interface_info.getName()));
      if (it == network_adapters.end()) {
        network_adapters.emplace_back(std::move(interface_info));
      } else {
        interface_info.moveAddressesInto(*it);
      }
    }
    if (max_interfaces.has_value() && network_adapters.size() >= max_interfaces.value())
      return network_adapters;
    adapter = adapter->Next;
  }
#else
  struct ifaddrs* interface_addresses = nullptr;
  auto cleanup = gsl::finally([&interface_addresses] { freeifaddrs(interface_addresses); });
  if (getifaddrs(&interface_addresses) == -1) {
    logger_->log_error("getifaddrs failed: {}", std::strerror(errno));
    return network_adapters;
  }

  for (struct ifaddrs* ifa = interface_addresses; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr)
      continue;
    NetworkInterfaceInfo interface_info(ifa);
    if (filter(interface_info)) {
      auto it = std::find_if(network_adapters.begin(), network_adapters.end(), HasName(interface_info.getName()));
      if (it == network_adapters.end()) {
        network_adapters.emplace_back(std::move(interface_info));
      } else {
        interface_info.moveAddressesInto(*it);
      }
    }
    if (max_interfaces.has_value() && network_adapters.size() >= max_interfaces.value())
      return network_adapters;
  }
#endif
  return network_adapters;
}

namespace {
void move_append(std::vector<std::string> &&source, std::vector<std::string> &destination) {
  destination.reserve(destination.size() + source.size());
  std::move(std::begin(source), std::end(source), std::back_inserter(destination));
  source.clear();
}
}  // namespace

void NetworkInterfaceInfo::moveAddressesInto(NetworkInterfaceInfo& destination) {
  move_append(std::move(ip_v4_addresses_), destination.ip_v4_addresses_);
  move_append(std::move(ip_v6_addresses_), destination.ip_v6_addresses_);
}

}  // namespace org::apache::nifi::minifi::utils
