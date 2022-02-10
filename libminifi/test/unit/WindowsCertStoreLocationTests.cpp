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

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("WindowsCertStoreLocation can be created from valid strings", "[WindowsCertStoreLocation][constructor]") {
  REQUIRE_NOTHROW(utils::tls::WindowsCertStoreLocation{"CurrentUser"});
  REQUIRE_NOTHROW(utils::tls::WindowsCertStoreLocation{"LocalMachine"});
}

TEST_CASE("WindowsCertStoreLocation cannot be created from invalid strings", "[WindowsCertStoreLocation][constructor]") {
  REQUIRE_THROWS(utils::tls::WindowsCertStoreLocation{"SomebodyElsesComputer"});
}

TEST_CASE("WindowsCertStoreLocation translates the common location strings to bitfield values correctly", "[WindowsCertStoreLocation][getBitfieldValue]") {
  REQUIRE(utils::tls::WindowsCertStoreLocation{"CurrentUser"}.getBitfieldValue() == CERT_SYSTEM_STORE_CURRENT_USER);
  REQUIRE(utils::tls::WindowsCertStoreLocation{"LocalMachine"}.getBitfieldValue() == CERT_SYSTEM_STORE_LOCAL_MACHINE);
}

TEST_CASE("LocalMachine is the default WindowsCertStoreLocation", "[WindowsCertStoreLocation][defaultLocation]") {
  REQUIRE(utils::tls::WindowsCertStoreLocation::defaultLocation() == "LocalMachine");
}

TEST_CASE("CurrentUser and LocalMachine are among the allowed values of WindowsCertStoreLocation", "[WindowsCertStoreLocation][allowedLocations]") {
  const std::set<std::string> allowed_locations = utils::tls::WindowsCertStoreLocation::allowedLocations();
  REQUIRE(allowed_locations.count("CurrentUser") == 1);
  REQUIRE(allowed_locations.count("LocalMachine") == 1);
  REQUIRE(allowed_locations.count("SomebodyElsesComputer") == 0);
}

#endif  // WIN32
