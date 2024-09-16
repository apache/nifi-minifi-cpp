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
#ifdef WIN32

#include <windows.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::utils::tls {

struct WindowsCertStoreLocation {
  static constexpr std::array<std::pair<std::string_view, DWORD>, 8> SYSTEM_STORE_LOCATIONS{{
    {"CurrentUser", CERT_SYSTEM_STORE_CURRENT_USER},
    {"LocalMachine", CERT_SYSTEM_STORE_LOCAL_MACHINE},
    {"CurrentService", CERT_SYSTEM_STORE_CURRENT_SERVICE},
    {"Services", CERT_SYSTEM_STORE_SERVICES},
    {"Users", CERT_SYSTEM_STORE_USERS},
    {"CurrentUserGroupPolicy", CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY},
    {"LocalMachineGroupPolicy", CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY},
    {"LocalMachineEnterprise", CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE}
  }};
  static constexpr size_t SIZE = SYSTEM_STORE_LOCATIONS.size();
  static constexpr auto LOCATION_NAMES = utils::getKeys(SYSTEM_STORE_LOCATIONS);
  static constexpr std::string_view DEFAULT_LOCATION = "LocalMachine";

  explicit constexpr WindowsCertStoreLocation(std::string_view location_name) : location_bitfield_value(utils::at(SYSTEM_STORE_LOCATIONS, location_name)) {}

  DWORD location_bitfield_value;
};

}  // namespace org::apache::nifi::minifi::utils::tls

#endif  // WIN32
