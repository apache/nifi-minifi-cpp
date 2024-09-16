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
#include "core/state/nodes/DeviceInformation.h"

#include <fstream>
#include <set>
#include <array>

#include "core/Resource.h"
#include "utils/net/NetworkInterfaceInfo.h"
#include "utils/OsUtils.h"
#include "core/state/Value.h"

namespace org::apache::nifi::minifi::state::response {

utils::SystemCpuUsageTracker DeviceInfoNode::cpu_load_tracker_;
std::mutex DeviceInfoNode::cpu_load_tracker_mutex_;

Device::Device() {
  std::array<char, 1024> hostname{};
  gethostname(hostname.data(), 1023);

  std::ifstream device_id_file(".device_id");
  if (device_id_file) {
    std::string line;
    while (device_id_file) {
      if (std::getline(device_id_file, line))
        device_id_ += line;
    }
    device_id_file.close();
  } else {
    device_id_ = getDeviceId();

    std::ofstream outputFile(".device_id");
    if (outputFile) {
      outputFile.write(device_id_.c_str(), gsl::narrow<std::streamsize>(device_id_.length()));
    }
    outputFile.close();
  }

  canonical_hostname_ = hostname.data();

  std::stringstream ips;
  auto ipaddressess = getIpAddresses();
  for (const auto& ip : ipaddressess) {
    if (ipaddressess.size() > 1 && (ip.find("127") == 0 || ip.find("192") == 0))
      continue;
    ip_ = ip;
    break;
  }
}

std::vector<std::string> Device::getIpAddresses() {
  static std::vector<std::string> ips;
  if (ips.empty()) {
    const auto filter = [](const utils::NetworkInterfaceInfo& interface_info) {
      return !interface_info.isLoopback() && interface_info.isRunning();
    };
    auto network_interface_infos = utils::NetworkInterfaceInfo::getNetworkInterfaceInfos(filter);
    for (const auto& network_interface_info : network_interface_infos)
      for (const auto& ip_v4_address : network_interface_info.getIpV4Addresses())
        ips.push_back(ip_v4_address);
  }
  return ips;
}

#if __linux__
std::string Device::getDeviceId() {
  std::hash<std::string> hash_fn;
  std::string macs;
  ifaddrs *ifaddr = nullptr;
  ifaddrs *ifa = nullptr;
  int family = 0;
  int s = 0;
  int n = 0;
  std::array<char, NI_MAXHOST> host{};

  if (getifaddrs(&ifaddr) == -1) {
    exit(EXIT_FAILURE);
  }

  /* Walk through linked list, maintaining head pointer so we
    can free list later */
  for (ifa = ifaddr, n = 0; ifa != nullptr; ifa = ifa->ifa_next, n++) {
    if (ifa->ifa_addr == nullptr)
      continue;

    family = ifa->ifa_addr->sa_family;

    /* Display interface name and family (including symbolic
      form of the latter for the common families) */

    /* For an AF_INET* interface address, display the address */

    if (family == AF_INET || family == AF_INET6) {
      s = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host.data(), NI_MAXHOST,
          nullptr,
          0, NI_NUMERICHOST);
      if (s != 0) {
        printf("getnameinfo() failed: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
      }
    }
  }

  freeifaddrs(ifaddr);

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  struct ifreq ifr{};
  struct ifconf ifc{};
  std::array<char, 1024> buf{};
  ifc.ifc_len = buf.size();
  ifc.ifc_buf = buf.data();
  if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */
  }

  struct ifreq* it = ifc.ifc_req;
  const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

  for (; it != end; ++it) {
    strcpy(ifr.ifr_name, it->ifr_name); // NOLINT
    if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
      if (!(ifr.ifr_flags & IFF_LOOPBACK)) {  // don't count loopback
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
          std::array<unsigned char, 6> mac{};

          memcpy(mac.data(), ifr.ifr_hwaddr.sa_data, mac.size());

          std::array<char, 13> mac_add{};
          snprintf(mac_add.data(), mac_add.size(), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); // NOLINT

          macs += mac_add.data();
        }
      }

    } else { /* handle error */
    }
  }

  close(sock);

  return std::to_string(hash_fn(macs));
}
#elif(defined(__unix__) || defined(__APPLE__) || defined(__MACH__) || defined(BSD))  // should work on bsd variants as well
std::string Device::getDeviceId() {
  ifaddrs* iflist;
  std::hash<std::string> hash_fn;
  std::set<std::string> macs;

  if (getifaddrs(&iflist) == 0) {
    for (ifaddrs* cur = iflist; cur; cur = cur->ifa_next) {
      if (cur->ifa_addr && (cur->ifa_addr->sa_family == AF_LINK) && (reinterpret_cast<sockaddr_dl*>(cur->ifa_addr))->sdl_alen) {
        sockaddr_dl* sdl = reinterpret_cast<sockaddr_dl*>(cur->ifa_addr);

        if (sdl->sdl_type != IFT_ETHER) {
          continue;
        } else {
        }
        char mac[32];
        memcpy(mac, LLADDR(sdl), sdl->sdl_alen);
        char mac_add[13];
        snprintf(mac_add, 13, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); // NOLINT
        // macs += mac_add;
        macs.insert(mac_add);
      }
    }

    freeifaddrs(iflist);
  }
  std::string macstr;
  for (auto &mac : macs) {
    macstr += mac;
  }
  return macstr.length() > 0 ? std::to_string(hash_fn(macstr)) : "8675309";
}
#else
std::string Device::getDeviceId() {
  PIP_ADAPTER_INFO adapterPtr;
  PIP_ADAPTER_INFO adapter = nullptr;

  DWORD dwRetVal = 0;

  std::hash<std::string> hash_fn;
  std::set<std::string> macs;

  ULONG adapterLen = sizeof(IP_ADAPTER_INFO);
  adapterPtr = reinterpret_cast<IP_ADAPTER_INFO*>(malloc(sizeof(IP_ADAPTER_INFO)));
  if (adapterPtr == nullptr) {
    return "";
  }
  if (GetAdaptersInfo(adapterPtr, &adapterLen) == ERROR_BUFFER_OVERFLOW) {
    free(adapterPtr);
    adapterPtr = reinterpret_cast<IP_ADAPTER_INFO*>(malloc(adapterLen));
    if (adapterPtr == nullptr) {
      return "";
    }
  }

  if ((dwRetVal = GetAdaptersInfo(adapterPtr, &adapterLen)) == NO_ERROR) {
    adapter = adapterPtr;
    while (adapter) {
      char mac_add[13];
      snprintf(mac_add, 13, "%02X%02X%02X%02X%02X%02X", adapter->Address[0], adapter->Address[1], adapter->Address[2], adapter->Address[3], adapter->Address[4], adapter->Address[5]); // NOLINT
      macs.insert(mac_add);
      adapter = adapter->Next;
    }
  }

  if (adapterPtr)
  free(adapterPtr);
  std::string macstr;
  for (auto &mac : macs) {
    macstr += mac;
  }
  return macstr.length() > 0 ? std::to_string(hash_fn(macstr)) : "8675309";
}
#endif

std::vector<SerializedResponseNode> DeviceInfoNode::serialize() {
  return {serializeIdentifier(), serializeSystemInfo(), serializeNetworkInfo()};
}

std::vector<PublishedMetric> DeviceInfoNode::calculateMetrics() {
  double system_cpu_usage = -1.0;
  {
    std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
    system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
  }
  SerializedResponseNode cpu_usage;
  cpu_usage.name = "cpuUtilization";
  cpu_usage.value = system_cpu_usage;
  std::vector<PublishedMetric> metrics = {
    {"physical_mem", static_cast<double>(utils::OsUtils::getSystemTotalPhysicalMemory()), {{"metric_class", "DeviceInfoNode"}}},
    {"memory_usage", static_cast<double>(utils::OsUtils::getSystemPhysicalMemoryUsage()), {{"metric_class", "DeviceInfoNode"}}},
    {"cpu_utilization", system_cpu_usage, {{"metric_class", "DeviceInfoNode"}}},
  };

  if (auto system_load_average = utils::OsUtils::getSystemLoadAverage()) {
    metrics.push_back({"cpu_load_average", *system_load_average, {{"metric_class", "DeviceInfoNode"}}});
  }

  return metrics;
}

SerializedResponseNode DeviceInfoNode::serializeIdentifier() const {
  return {.name = "identifier", .value = device_id_};
}

SerializedResponseNode DeviceInfoNode::serializeVCoreInfo() {
  return {.name = "vCores", .value = std::thread::hardware_concurrency()};
}

SerializedResponseNode DeviceInfoNode::serializeOperatingSystemType() {
  return {.name = "operatingSystem", .value = getOperatingSystem()};
}

SerializedResponseNode DeviceInfoNode::serializeTotalPhysicalMemoryInformation() {
  return {.name = "physicalMem", .value = utils::OsUtils::getSystemTotalPhysicalMemory()};
}

SerializedResponseNode DeviceInfoNode::serializePhysicalMemoryUsageInformation() {
  SerializedResponseNode used_physical_memory;
  used_physical_memory.name = "memoryUsage";
  used_physical_memory.value = utils::OsUtils::getSystemPhysicalMemoryUsage();
  return used_physical_memory;
}

SerializedResponseNode DeviceInfoNode::serializeSystemCPUUsageInformation() {
  double system_cpu_usage = -1.0;
  {
    std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
    system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
  }
  return {.name = "cpuUtilization", .value = system_cpu_usage};
}

SerializedResponseNode DeviceInfoNode::serializeArchitectureInformation() {
  return {.name = "machineArch", .value = utils::OsUtils::getMachineArchitecture()};
}

std::optional<SerializedResponseNode> DeviceInfoNode::serializeCPULoadAverageInformation() {
  if (auto system_load_average = utils::OsUtils::getSystemLoadAverage()) {
    return SerializedResponseNode{.name = "cpuLoadAverage", .value = *system_load_average};
  }

  return std::nullopt;
}

SerializedResponseNode DeviceInfoNode::serializeSystemInfo() {
  SerializedResponseNode system_info = {
    .name = "systemInfo",
    .children = {
      serializeVCoreInfo(),
      serializeOperatingSystemType(),
      serializeTotalPhysicalMemoryInformation(),
      serializeArchitectureInformation(),
      serializePhysicalMemoryUsageInformation(),
      serializeSystemCPUUsageInformation()
    }
  };

  if (auto cpu_load_average = serializeCPULoadAverageInformation()) {
    system_info.children.push_back(*cpu_load_average);
  }
  return system_info;
}

SerializedResponseNode DeviceInfoNode::serializeHostNameInfo() const {
  return {.name = "hostname", .value = hostname_};
}

SerializedResponseNode DeviceInfoNode::serializeIPAddress() const {
  return {.name = "ipAddress", .value = !ip_.empty() ? ip_ : "127.0.0.1"};
}

SerializedResponseNode DeviceInfoNode::serializeNetworkInfo() const {
  return {.name = "networkInfo", .children = { serializeHostNameInfo(), serializeIPAddress()}};
}

REGISTER_RESOURCE(DeviceInfoNode, DescriptionOnly);

}  // namespace org::apache::nifi::minifi::state::response
