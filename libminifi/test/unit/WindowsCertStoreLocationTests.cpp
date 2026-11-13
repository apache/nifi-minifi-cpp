// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <windows.h>
#include <wincrypt.h>

#include "unit/TestUtils.h"
#include "unit/Catch.h"
#include "utils/tls/WindowsCertStoreLocation.h"
#include "range/v3/algorithm/contains.hpp"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("WindowsCertStoreLocation can be created from valid strings", "[WindowsCertStoreLocation][constructor]") {
  CHECK_NOTHROW(utils::tls::WindowsCertStoreLocation{"CurrentUser"});
  CHECK_NOTHROW(utils::tls::WindowsCertStoreLocation{"LocalMachine"});
}

TEST_CASE("WindowsCertStoreLocation cannot be created from invalid strings", "[WindowsCertStoreLocation][constructor]") {
  CHECK_THROWS(utils::tls::WindowsCertStoreLocation{"SomebodyElsesComputer"});
}

TEST_CASE("WindowsCertStoreLocation translates the common location strings to bitfield values correctly", "[WindowsCertStoreLocation][location_bitfield_value]") {
  CHECK(utils::tls::WindowsCertStoreLocation{"CurrentUser"}.location_bitfield_value == CERT_SYSTEM_STORE_CURRENT_USER);
  CHECK(utils::tls::WindowsCertStoreLocation{"LocalMachine"}.location_bitfield_value == CERT_SYSTEM_STORE_LOCAL_MACHINE);
}

TEST_CASE("LocalMachine is the default WindowsCertStoreLocation", "[WindowsCertStoreLocation][defaultLocation]") {
  CHECK(utils::tls::WindowsCertStoreLocation::DEFAULT_LOCATION == "LocalMachine");
}

TEST_CASE("CurrentUser and LocalMachine are among the allowed values of WindowsCertStoreLocation", "[WindowsCertStoreLocation][LOCATION_NAMES]") {
  const auto& allowed_locations = utils::tls::WindowsCertStoreLocation::LOCATION_NAMES;
  CHECK(ranges::contains(allowed_locations, "CurrentUser"));
  CHECK(ranges::contains(allowed_locations, "LocalMachine"));
  CHECK_FALSE(ranges::contains(allowed_locations, "SomebodyElsesComputer"));
}
