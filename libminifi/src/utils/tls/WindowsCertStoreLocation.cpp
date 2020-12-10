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
#ifdef WIN32

#include "utils/tls/WindowsCertStoreLocation.h"

#include <wincrypt.h>

#include <array>
#include <utility>
#include <stdexcept>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")

namespace {

constexpr std::array<std::pair<const char*, DWORD>, 8> SYSTEM_STORE_LOCATIONS{{
    {"CurrentUser", CERT_SYSTEM_STORE_CURRENT_USER},
    {"LocalMachine", CERT_SYSTEM_STORE_LOCAL_MACHINE},
    {"CurrentService", CERT_SYSTEM_STORE_CURRENT_SERVICE},
    {"Services", CERT_SYSTEM_STORE_SERVICES},
    {"Users", CERT_SYSTEM_STORE_USERS},
    {"CurrentUserGroupPolicy", CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY},
    {"LocalMachineGroupPolicy", CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY},
    {"LocalMachineEnterprise", CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE}
}};

constexpr const char* DEFAULT_LOCATION = "LocalMachine";

DWORD parseLocationName(const std::string& location_name) {
  for (const auto& kv : SYSTEM_STORE_LOCATIONS) {
    if (location_name == kv.first) {
      return kv.second;
    }
  }
  throw std::runtime_error{"Unknown Windows system certificate store name: " + location_name};
}

}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace tls {

WindowsCertStoreLocation::WindowsCertStoreLocation(const std::string& location_name)
    : location_bitfield_value_(parseLocationName(location_name)) {
}

std::string WindowsCertStoreLocation::defaultLocation() {
  return DEFAULT_LOCATION;
}

// Returns a set because the return value is used as the parameter of PropertyBuilder::withAllowableValues()
std::set<std::string> WindowsCertStoreLocation::allowedLocations() {
  std::set<std::string> allowed_locations;
  for (const auto& kv : SYSTEM_STORE_LOCATIONS) {
    allowed_locations.emplace(kv.first);
  }
  return allowed_locations;
}

}  // namespace tls
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // WIN32
