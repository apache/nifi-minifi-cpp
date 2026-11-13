// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
