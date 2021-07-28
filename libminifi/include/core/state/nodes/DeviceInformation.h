/**
 *
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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_DEVICEINFORMATION_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_DEVICEINFORMATION_H_

#ifndef WIN32
#if ( defined(__APPLE__) || defined(__MACH__) || defined(BSD))
#include <net/if_dl.h>
#include <net/if_types.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>

#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>

#else
#pragma comment(lib, "iphlpapi.lib")
#include <iphlpapi.h>

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "io/ClientSocket.h"
#include "utils/OsUtils.h"
#include "utils/NetworkInterfaceInfo.h"
#include "utils/SystemCpuUsageTracker.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

class Device {
 public:
  Device() {
    initialize();
  }
  void initialize() {
    addrinfo hints;
    memset(&hints, 0, sizeof hints);  // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_protocol = 0; /* any protocol */

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

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
        outputFile.write(device_id_.c_str(), device_id_.length());
      }
      outputFile.close();
    }

    canonical_hostname_ = hostname;

    std::stringstream ips;
    auto ipaddressess = getIpAddresses();
    for (auto ip : ipaddressess) {
      if (ipaddressess.size() > 1 && (ip.find("127") == 0 || ip.find("192") == 0))
        continue;
      ip_ = ip;
      break;
    }
  }

  std::string canonical_hostname_;
  std::string ip_;
  std::string device_id_;

 protected:
  std::vector<std::string> getIpAddresses() {
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
  std::string getDeviceId() {
    std::hash<std::string> hash_fn;
    std::string macs;
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
      exit(EXIT_FAILURE);
    }

    /* Walk through linked list, maintaining head pointer so we
     can free list later */
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
      if (ifa->ifa_addr == NULL)
        continue;

      family = ifa->ifa_addr->sa_family;

      /* Display interface name and family (including symbolic
       form of the latter for the common families) */

      /* For an AF_INET* interface address, display the address */

      if (family == AF_INET || family == AF_INET6) {
        s = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST,
            NULL,
            0, NI_NUMERICHOST);
        if (s != 0) {
          printf("getnameinfo() failed: %s\n", gai_strerror(s));
          exit(EXIT_FAILURE);
        }
      }
    }

    freeifaddrs(ifaddr);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
      strcpy(ifr.ifr_name, it->ifr_name); // NOLINT
      if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
        if (!(ifr.ifr_flags & IFF_LOOPBACK)) {  // don't count loopback
          if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char mac[6];

            memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);

            char mac_add[13];
            snprintf(mac_add, 13, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); // NOLINT

            macs += mac_add;
          }
        }

      } else { /* handle error */
      }
    }

    close(sock);

    return std::to_string(hash_fn(macs));
  }
#elif(defined(__unix__) || defined(__APPLE__) || defined(__MACH__) || defined(BSD))  // should work on bsd variants as well
  std::string getDeviceId() {
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
  std::string getDeviceId() {
    PIP_ADAPTER_INFO adapterPtr;
    PIP_ADAPTER_INFO adapter = NULL;

    DWORD dwRetVal = 0;

    std::hash<std::string> hash_fn;
    std::set<std::string> macs;

    ULONG adapterLen = sizeof(IP_ADAPTER_INFO);
    adapterPtr = reinterpret_cast<IP_ADAPTER_INFO*>(malloc(sizeof(IP_ADAPTER_INFO)));
    if (adapterPtr == NULL) {
      return "";
    }
    if (GetAdaptersInfo(adapterPtr, &adapterLen) == ERROR_BUFFER_OVERFLOW) {
      free(adapterPtr);
      adapterPtr = reinterpret_cast<IP_ADAPTER_INFO*>(malloc(adapterLen));
      if (adapterPtr == NULL) {
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

  // connection information
  int32_t socket_file_descriptor_;

  addrinfo *addr_info_;
};

/**
 * Justification and Purpose: Provides Device Information
 */
class DeviceInfoNode : public DeviceInformation {
 public:
  DeviceInfoNode(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
    static Device device;
    hostname_ = device.canonical_hostname_;
    ip_ = device.ip_;
    device_id_ = device.device_id_;
  }

  DeviceInfoNode(const std::string &name) // NOLINT
      : DeviceInformation(name) {
    static Device device;
    hostname_ = device.canonical_hostname_;
    ip_ = device.ip_;
    device_id_ = device.device_id_;
  }

  std::string getName() const {
    return "deviceInfo";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    serialized.push_back(serializeIdentifier());
    serialized.push_back(serializeSystemInfo());
    serialized.push_back(serializeNetworkInfo());

    return serialized;
  }

 protected:
  SerializedResponseNode serializeIdentifier() const {
    SerializedResponseNode identifier;
    identifier.name = "identifier";
    identifier.value = device_id_;
    return identifier;
  }

  SerializedResponseNode serializeVCoreInfo() const {
    SerializedResponseNode v_cores;
    v_cores.name = "vCores";
    v_cores.value = std::thread::hardware_concurrency();
    return v_cores;
  }

  SerializedResponseNode serializeOperatingSystemType() const {
    SerializedResponseNode os_type;
    os_type.name = "operatingSystem";
    os_type.value = getOperatingSystem();
    return os_type;
  }

  SerializedResponseNode serializeTotalPhysicalMemoryInformation() const {
    SerializedResponseNode total_physical_memory;
    total_physical_memory.name = "physicalMem";
    total_physical_memory.value = utils::OsUtils::getSystemTotalPhysicalMemory();
    return total_physical_memory;
  }

  SerializedResponseNode serializePhysicalMemoryUsageInformation() const {
    SerializedResponseNode used_physical_memory;
    used_physical_memory.name = "memoryUsage";
    used_physical_memory.value = utils::OsUtils::getSystemPhysicalMemoryUsage();
    return used_physical_memory;
  }

  SerializedResponseNode serializeSystemCPUUsageInformation() const {
    double system_cpu_usage = -1.0;
    {
      std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
      system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
    }
    SerializedResponseNode cpu_usage;
    cpu_usage.name = "cpuUtilization";
    cpu_usage.value = system_cpu_usage;
    return cpu_usage;
  }

  SerializedResponseNode serializeArchitectureInformation() const {
    SerializedResponseNode arch;
    arch.name = "machinearch";
    arch.value = utils::OsUtils::getMachineArchitecture();
    return arch;
  }

  SerializedResponseNode serializeSystemInfo() const {
    SerializedResponseNode systemInfo;
    systemInfo.name = "systemInfo";

    systemInfo.children.push_back(serializeVCoreInfo());
    systemInfo.children.push_back(serializeOperatingSystemType());
    systemInfo.children.push_back(serializeTotalPhysicalMemoryInformation());
    systemInfo.children.push_back(serializeArchitectureInformation());
    systemInfo.children.push_back(serializePhysicalMemoryUsageInformation());
    systemInfo.children.push_back(serializeSystemCPUUsageInformation());

    return systemInfo;
  }

  SerializedResponseNode serializeHostNameInfo() const {
    SerializedResponseNode hostname;
    hostname.name = "hostname";
    hostname.value = hostname_;
    return hostname;
  }

  SerializedResponseNode serializeIPAddress() const {
    SerializedResponseNode ip;
    ip.name = "ipAddress";
    ip.value = !ip_.empty() ? ip_ : "127.0.0.1";
    return ip;
  }

  SerializedResponseNode serializeNetworkInfo() const {
    SerializedResponseNode network_info;
    network_info.name = "networkInfo";
    network_info.children.push_back(serializeHostNameInfo());
    network_info.children.push_back(serializeIPAddress());
    return network_info;
  }

  /**
   * Have found various ways of identifying different operating system variants
   * so these were either pulled from header files or online.
   */
  static inline std::string getOperatingSystem() {
    /**
     * We define WIN32, but *most* compilers should provide _WIN32.
     */
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__APPLE__) || defined(__MACH__)
    return "Mac OSX";
#elif defined(__linux__)
    return "Linux";
#elif defined(__unix) || defined(__unix__) || defined(__FreeBSD__)
    return "Unix";
#else
    return "Other";
#endif
  }

  std::string hostname_;
  std::string ip_;
  std::string device_id_;
  MINIFIAPI static utils::SystemCpuUsageTracker cpu_load_tracker_;
  MINIFIAPI static std::mutex cpu_load_tracker_mutex_;
};

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_DEVICEINFORMATION_H_
