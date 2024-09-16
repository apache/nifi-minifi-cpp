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

#include "utils/OsUtils.h"

#include <iostream>
#include <map>

#include "fmt/format.h"
#include "utils/gsl.h"
#include "Exception.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#include <cstdlib>
#include <optional>
#include <sstream>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <sddl.h>
#include <psapi.h>
#include <winsock2.h>
#include <vector>
#include <algorithm>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/utsname.h>
#include <pwd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

namespace org::apache::nifi::minifi::utils {

#ifdef _WIN32
/*
 These are common translations for SIDs in windows
 */
std::string OsUtils::resolve_common_identifiers(const std::string &id) {
  static std::map<std::string, std::string> nameMap;
  if (nameMap.empty()) {
    nameMap["S-1-0"] = "Null Authority";
    nameMap["S-1-0-0"] = "Nobody";
    nameMap["S-1-1-0"] = "Everyone";
    nameMap["S-1-2"] = "Local Authority";
    nameMap["S-1-2-0"] = "Local";
    nameMap["S-1-2-1"] = "Console Logon";
    nameMap["S-1-3-0"] = "Creator Owner";
    nameMap["S-1-3-1"] = "Creator Group";
  }
  auto name = nameMap.find(id);
  if (name != std::end(nameMap)) {
    return name->second;
  }
  return "";
}
#endif

std::string OsUtils::userIdToUsername(const std::string &uid) {
  std::string name;
  name = uid;
  if (!name.empty()) {
#ifdef _WIN32
    auto resolved_name = resolve_common_identifiers(name);
    if (!resolved_name.empty()) {
      return resolved_name;
    }
    // First call to LookupAccountSid to get the buffer sizes.
    PSID pSidOwner = nullptr;
    const auto guard_pSidOwner = gsl::finally([&pSidOwner]() { if (pSidOwner != nullptr) { LocalFree(pSidOwner); } });
    if (ConvertStringSidToSidA(name.c_str(), &pSidOwner)) {
      SID_NAME_USE sidType = SidTypeUnknown;
      DWORD windowsAccountNameSize = 0;
      DWORD dwwindowsDomainSize = 0;
      /*
       We can use a unique ptr with a deleter here but some of the calls
       below require we use global alloc -- so a global deleter to call GlobalFree
       won't buy us a ton unless we anticipate requiring more of this. If we do
       I suggest we break this out until a subset of OsUtils into our own convenience functions.
       */
      LPTSTR windowsDomain = nullptr;
      LPTSTR windowsAccount = nullptr;

      /*
       The first call will be to obtain sizes for domain and account,
       after which we will allocate the memory and free it after.
       In some cases youc an replace GlobalAlloc with
       */
      LookupAccountSid(nullptr /** local computer **/, pSidOwner,
          windowsAccount,
          (LPDWORD)&windowsAccountNameSize,
          windowsDomain,
          (LPDWORD)&dwwindowsDomainSize,
          &sidType);

      if (windowsAccountNameSize > 0) {
        windowsAccount = (LPTSTR)GlobalAlloc(
            GMEM_FIXED,
            windowsAccountNameSize);

        if (dwwindowsDomainSize > 0) {
          windowsDomain = (LPTSTR)GlobalAlloc(
              GMEM_FIXED,
              dwwindowsDomainSize);
        }

        if (LookupAccountSid(
                nullptr,
                pSidOwner,
                windowsAccount,
                (LPDWORD)&windowsAccountNameSize,
                windowsDomain,
                (LPDWORD)&dwwindowsDomainSize,
                &sidType)) {
          if (dwwindowsDomainSize > 0) {
            std::string domain = std::string(windowsDomain);
            name = domain + "\\";
            name += std::string(windowsAccount);
          } else {
            name = std::string(windowsAccount);
          }
        }
        GlobalFree(windowsAccount);
        GlobalFree(windowsDomain);
      }
    }
#else
    auto ptr = name.c_str();
    char *end = nullptr;  // it will be unused
    uid_t ret = std::strtol(ptr, &end, 10);
    if (ret > 0) {
      struct passwd pass{};
      struct passwd *result = nullptr;
      std::array<char, 1024> localbuf{};
      if (!getpwuid_r(ret, &pass, localbuf.data(), localbuf.size(), &result)) {
        name = pass.pw_name;
      }
    }
#endif
  }
  return name;
}

int64_t OsUtils::getCurrentProcessPhysicalMemoryUsage() {
#if defined(__linux__)
  static const std::string resident_set_size_prefix = "VmRSS:";
  std::ifstream status_file("/proc/self/status");
  std::string line;

  while (std::getline(status_file, line)) {
    if (line.rfind(resident_set_size_prefix, 0) == 0) {
      std::istringstream resident_set_size_value(line.substr(resident_set_size_prefix.length()));
      uint64_t memory_usage_in_kBytes = 0;
      resident_set_size_value >> memory_usage_in_kBytes;
      return gsl::narrow<int64_t>(memory_usage_in_kBytes * 1024);
    }
  }

  return -1;
#elif defined(__APPLE__)
  task_basic_info tInfo;
  mach_msg_type_number_t tInfoCount = TASK_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&tInfo, &tInfoCount))
    return -1;
  return tInfo.resident_size;
#elif defined(WIN32)
  PROCESS_MEMORY_COUNTERS pmc;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    return -1;
  return pmc.WorkingSetSize;
#else
#warning "Unsupported platform"
  return -1;
#endif
}

int64_t OsUtils::getCurrentProcessId() {
#ifdef WIN32
  return int64_t{GetCurrentProcessId()};
#else
  return int64_t{getpid()};
#endif
}

int64_t OsUtils::getSystemPhysicalMemoryUsage() {
#if defined(__linux__)
  const std::string available_memory_prefix = "MemAvailable:";
  const std::string total_memory_prefix = "MemTotal:";
  std::ifstream meminfo_file("/proc/meminfo");
  std::string line;

  std::optional<uint64_t> total_memory_kByte;
  std::optional<uint64_t> available_memory_kByte;
  while ((!total_memory_kByte.has_value() || !available_memory_kByte.has_value()) && std::getline(meminfo_file, line)) {
    if (line.rfind(total_memory_prefix, 0) == 0) {
      std::istringstream total_memory_line(line.substr(total_memory_prefix.length()));
      total_memory_kByte.emplace(0);
      total_memory_line >> total_memory_kByte.value();
    } else if (line.rfind(available_memory_prefix, 0) == 0) {
      std::istringstream available_memory_line(line.substr(available_memory_prefix.length()));
      available_memory_kByte.emplace(0);
      available_memory_line >> available_memory_kByte.value();
    }
  }
  if (total_memory_kByte.has_value() && available_memory_kByte.has_value())
    return (gsl::narrow<int64_t>(total_memory_kByte.value()) - gsl::narrow<int64_t>(available_memory_kByte.value())) * 1024;

  return -1;
#elif defined(__APPLE__)
  vm_size_t page_size;
  mach_port_t mach_port = mach_host_self();
  vm_statistics64_data_t vm_stats;
  mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
  if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
      KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                                      (host_info64_t)&vm_stats, &count)) {
      uint64_t physical_memory_used = ((int64_t)vm_stats.active_count +
                               (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
      return physical_memory_used;
  }
  return -1;
#elif defined(WIN32)
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memory_info);
  DWORDLONG physical_memory_used = memory_info.ullTotalPhys - memory_info.ullAvailPhys;
  return physical_memory_used;
#else
#warning "Unsupported platform"
  return -1;
#endif
}

int64_t OsUtils::getSystemTotalPhysicalMemory() {
#if defined(__linux__)
  struct sysinfo memory_info{};
  sysinfo(&memory_info);
  uint64_t total_physical_memory = memory_info.totalram;
  total_physical_memory *= memory_info.mem_unit;
  return gsl::narrow<int64_t>(total_physical_memory);
#elif defined(__APPLE__)
  int mib[2];
  int64_t total_physical_memory = 0;
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  size_t length = sizeof(int64_t);
  sysctl(mib, 2, &total_physical_memory, &length, NULL, 0);
  return total_physical_memory;
#elif defined(WIN32)
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memory_info);
  DWORDLONG total_physical_memory = memory_info.ullTotalPhys;
  return total_physical_memory;
#else
#warning "Unsupported platform"
  return -1;
#endif
}

#ifdef WIN32
int64_t OsUtils::getTotalPagingFileSize() {
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memory_info);
  DWORDLONG total_paging_file_size = memory_info.ullTotalPageFile;
  return total_paging_file_size;
}

std::error_code OsUtils::windowsErrorToErrorCode(DWORD error_code) {
  return {gsl::narrow_cast<int>(error_code), std::system_category()};
}
#endif

std::string OsUtils::getMachineArchitecture() {
#if defined(WIN32)
  SYSTEM_INFO system_information;
  GetNativeSystemInfo(&system_information);
  switch (system_information.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      return "x32";
    case PROCESSOR_ARCHITECTURE_AMD64:
      return "x64";
    case PROCESSOR_ARCHITECTURE_ARM:
      return "arm32";
    case PROCESSOR_ARCHITECTURE_ARM64:
      return "arm64";
    case PROCESSOR_ARCHITECTURE_IA64:
      return "x64";
    default:
      return "unknown";
  }
#else
  utsname buf{};
  if (uname(&buf) == -1)
    return "unknown";
  else
    return buf.machine;
#endif

  return "unknown";
}

std::optional<std::string> OsUtils::getHostName() {
  std::array<char, 1024> hostname{};
  if (gethostname(hostname.data(), 1023) != 0) {
    return std::nullopt;
  }
  return {hostname.data()};
}

std::optional<double> OsUtils::getSystemLoadAverage() {
#ifndef WIN32
  std::array<double, 1> load_avg{};
  auto numSamples = getloadavg(load_avg.data(), 1);
  if (numSamples == -1) {
    return std::nullopt;
  }
  return load_avg[0];
#else
  return std::nullopt;
#endif
}

}  // namespace org::apache::nifi::minifi::utils
