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

#ifdef WIN32
#include <Windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#pragma comment(lib, "IPHLPAPI.lib")
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#ifdef WIN32
std::string utf8_encode(const std::wstring& wstr) {
  if (wstr.empty())
    return std::string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], wstr.size(), nullptr, 0, nullptr, nullptr);
  std::string result_string(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], wstr.size(), &result_string[0], size_needed, nullptr, nullptr);
  return result_string;
}

NetworkInterfaceInfo::NetworkInterfaceInfo(const IP_ADAPTER_ADDRESSES* adapter) {
  name_ = utf8_encode(adapter->FriendlyName);
  for (auto unicast_address = adapter->FirstUnicastAddress; unicast_address != nullptr; unicast_address = unicast_address->Next) {
    if (unicast_address->Address.lpSockaddr->sa_family == AF_INET) {
      char address_buffer[INET_ADDRSTRLEN];
      void* sin_address = &(reinterpret_cast<SOCKADDR_IN*>(unicast_address->Address.lpSockaddr)->sin_addr);
      InetNtopA(AF_INET, sin_address, address_buffer, INET_ADDRSTRLEN);
      ip_v4_addresses_.push_back(address_buffer);
    } else if (unicast_address->Address.lpSockaddr->sa_family == AF_INET6) {
      char address_buffer[INET6_ADDRSTRLEN];
      void* sin_address = &(reinterpret_cast<SOCKADDR_IN*>(unicast_address->Address.lpSockaddr)->sin_addr);
      InetNtopA(AF_INET6, sin_address, address_buffer, INET6_ADDRSTRLEN);
      ip_v6_addresses_.push_back(address_buffer);
    }
  }
  running_ = adapter->OperStatus == IfOperStatusUp;
  loopback_ = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
}
#else
NetworkInterfaceInfo::NetworkInterfaceInfo(const struct ifaddrs* ifa) {
  name_ = ifa->ifa_name;
  void* sin_address = &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr)->sin_addr);
  if (ifa->ifa_addr->sa_family == AF_INET) {
    char address_buffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, sin_address, address_buffer, INET_ADDRSTRLEN);
    ip_v4_addresses_.push_back(address_buffer);
  } else if (ifa->ifa_addr->sa_family == AF_INET6) {
    char address_buffer[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, sin_address, address_buffer, INET6_ADDRSTRLEN);
    ip_v6_addresses_.push_back(address_buffer);
  }
  running_ = (ifa->ifa_flags & IFF_RUNNING);
  loopback_ = (ifa->ifa_flags & IFF_LOOPBACK);
}
#endif

std::unordered_map<std::string, NetworkInterfaceInfo> NetworkInterfaceInfo::getNetworkInterfaceInfos(std::function<bool(const NetworkInterfaceInfo&)> filter,
                                                                                                     const utils::optional<uint32_t> max_interfaces) {
  std::unordered_map<std::string, NetworkInterfaceInfo> network_adapters;
#ifdef WIN32
  ULONG buffer_length = sizeof(IP_ADAPTER_ADDRESSES);
  if (ERROR_BUFFER_OVERFLOW != GetAdaptersAddresses(0, 0, nullptr, nullptr, &buffer_length))
    return network_adapters;
  std::vector<uint8_t> bytes(buffer_length, 0);
  IP_ADAPTER_ADDRESSES* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(bytes.data());
  if (NO_ERROR == GetAdaptersAddresses(0, 0, nullptr, adapter, &buffer_length)) {
    while (adapter != nullptr) {
      NetworkInterfaceInfo interface_info(adapter);
      if (filter(interface_info)) {
        if (network_adapters.find(interface_info.getName()) == network_adapters.end()) {
          network_adapters.emplace(interface_info.getName(), std::move(interface_info));
        } else {
          interface_info.moveAddressesInto(network_adapters.at(interface_info.getName()));
        }
      }
      if (max_interfaces.has_value() && network_adapters.size() >= max_interfaces.value())
        return network_adapters;
      adapter = adapter->Next;
    }
  }
#else
  struct ifaddrs* interface_addresses = nullptr;
  auto cleanup = gsl::finally([interface_addresses] { freeifaddrs(interface_addresses); });
  if (getifaddrs(&interface_addresses) == -1)
    return network_adapters;

  for (struct ifaddrs* ifa = interface_addresses; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr)
      continue;
    NetworkInterfaceInfo interface_info(ifa);
    if (filter(interface_info)) {
      if (network_adapters.find(interface_info.getName()) == network_adapters.end()) {
        network_adapters.emplace(interface_info.getName(), std::move(interface_info));
      } else {
        interface_info.moveAddressesInto(network_adapters.at(interface_info.getName()));
      }
    }
    if (max_interfaces.has_value() && network_adapters.size() >= max_interfaces.value())
      return network_adapters;
  }
#endif
  return network_adapters;
}

void move_append(std::vector<std::string>& source, std::vector<std::string>& destination) {
  if (source.size() == 0)
    return;
  if (destination.empty()) {
    destination = std::move(source);
  } else {
    destination.reserve(destination.size() + source.size());
    std::move(std::begin(source), std::end(source), std::back_inserter(destination));
    source.clear();
  }
}

void NetworkInterfaceInfo::moveAddressesInto(NetworkInterfaceInfo& destination) {
  move_append(ip_v4_addresses_, destination.ip_v4_addresses_);
  move_append(ip_v6_addresses_, destination.ip_v6_addresses_);
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
