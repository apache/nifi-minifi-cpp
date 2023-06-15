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

#include <windows.h>
#include <wincrypt.h>

#include "TestUtils.h"
#include "../Catch.h"
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

#endif  // WIN32
