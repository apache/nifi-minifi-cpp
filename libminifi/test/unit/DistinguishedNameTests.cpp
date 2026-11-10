// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/tls/DistinguishedName.h"
#include "unit/Catch.h"

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
