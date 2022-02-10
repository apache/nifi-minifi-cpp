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

#include "utils/tls/DistinguishedName.h"
#include "../Catch.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("DistinguishedName can be created from a comma separated list", "[constructor]") {
  utils::tls::DistinguishedName dn = utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=minifi");
  REQUIRE(dn.toString() == "C=US, CN=minifi, O=Apache");
}

TEST_CASE("DistinguishedName can be created from a slash separated list", "[constructor]") {
  utils::tls::DistinguishedName dn = utils::tls::DistinguishedName::fromSlashSeparated("/C=US/O=Apache/CN=minifi");
  REQUIRE(dn.toString() == "C=US, CN=minifi, O=Apache");
}

TEST_CASE("DistinguishedName objects can be compared with ==", "[operator==]") {
  utils::tls::DistinguishedName dn1 = utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=minifi");
  utils::tls::DistinguishedName dn2 = utils::tls::DistinguishedName::fromCommaSeparated("O=Apache, CN=minifi, C=US");
  utils::tls::DistinguishedName dn3 = utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=MiNiFi");

  REQUIRE(dn1 == dn2);
  REQUIRE_FALSE(dn1 == dn3);
  REQUIRE_FALSE(dn2 == dn3);
}

TEST_CASE("DistinguishedName objects can be compared with !=", "[operator!=]") {
  utils::tls::DistinguishedName dn1 = utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=minifi");
  utils::tls::DistinguishedName dn2 = utils::tls::DistinguishedName::fromCommaSeparated("O=Apache, CN=minifi, C=US");
  utils::tls::DistinguishedName dn3 = utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=MiNiFi");

  REQUIRE_FALSE(dn1 != dn2);
  REQUIRE(dn1 != dn3);
  REQUIRE(dn2 != dn3);
}

TEST_CASE("DistinguishedName can return the CN component", "[getCN]") {
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=minifi").getCN().value_or("(none)") == "minifi");
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("O=Apache, CN=minifi, C=US").getCN().value_or("(none)") == "minifi");
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=MiNiFi").getCN().value_or("(none)") == "MiNiFi");
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, OU=NIFI").getCN().value_or("(none)") == "(none)");
}

TEST_CASE("DistinguishedName can be converted to string", "[toString]") {
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=minifi").toString() == "C=US, CN=minifi, O=Apache");
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("O=Apache, CN=minifi, C=US").toString() == "C=US, CN=minifi, O=Apache");
  REQUIRE(utils::tls::DistinguishedName::fromCommaSeparated("C=US, O=Apache, CN=MiNiFi").toString() == "C=US, CN=MiNiFi, O=Apache");
}
