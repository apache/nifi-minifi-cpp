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
#include <Windows.h>
#include <iphlpapi.h>
#include <WinSock2.h>

#endif

#include <string>
#include <vector>
#include <utility>
#include <optional>

#include "core/state/nodes/MetricsBase.h"
#include "utils/SystemCpuUsageTracker.h"
#include "minifi-cpp/utils/Export.h"

namespace org::apache::nifi::minifi::state::response {

class Device {
 public:
  Device();

  std::string canonical_hostname_;
  std::string ip_;
  std::string device_id_;

 protected:
  static std::vector<std::string> getIpAddresses();
  static std::string getDeviceId();

  // connection information
  int32_t socket_file_descriptor_ = 0;

  addrinfo *addr_info_ = nullptr;
};

/**
 * Justification and Purpose: Provides Device Information
 */
class DeviceInfoNode : public DeviceInformation {
 public:
  DeviceInfoNode(std::string_view name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
    static Device device;
    hostname_ = device.canonical_hostname_;
    ip_ = device.ip_;
    device_id_ = device.device_id_;
  }

  explicit DeviceInfoNode(std::string_view name)
      : DeviceInformation(name) {
    static Device device;
    hostname_ = device.canonical_hostname_;
    ip_ = device.ip_;
    device_id_ = device.device_id_;
  }

  MINIFIAPI static constexpr const char* Description = "Metric node that defines device characteristics to the C2 protocol";

  std::string getName() const override {
    return "deviceInfo";
  }

  std::vector<SerializedResponseNode> serialize() override;
  std::vector<PublishedMetric> calculateMetrics() override;

 protected:
  SerializedResponseNode serializeIdentifier() const;
  static SerializedResponseNode serializeVCoreInfo();
  static SerializedResponseNode serializeOperatingSystemType();
  static SerializedResponseNode serializeTotalPhysicalMemoryInformation();
  static SerializedResponseNode serializePhysicalMemoryUsageInformation();
  static SerializedResponseNode serializeSystemCPUUsageInformation();
  static std::optional<SerializedResponseNode> serializeCPULoadAverageInformation();
  static SerializedResponseNode serializeArchitectureInformation();
  static SerializedResponseNode serializeSystemInfo();
  SerializedResponseNode serializeHostNameInfo() const;
  SerializedResponseNode serializeIPAddress() const;
  SerializedResponseNode serializeNetworkInfo() const;

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

}  // namespace org::apache::nifi::minifi::state::response
