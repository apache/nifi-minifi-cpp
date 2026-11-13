// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string>
#include <vector>
#include "utils/ClassUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"

TEST_CASE("Test ShortNames", "[testcrc1]") {
  std::string className;
  std::string adjusted;
  SECTION("EMPTY") {
  className = "";
  adjusted = "";
  REQUIRE(!utils::ClassUtils::shortenClassName(className, adjusted));
  REQUIRE(adjusted.empty());
  }

  SECTION("SINGLE") {
  className = "Class";
  adjusted = "";
  // class name not shortened
  REQUIRE(!utils::ClassUtils::shortenClassName(className, adjusted));
  REQUIRE(adjusted.empty());
  className = "org::Test";
  adjusted = "";
  REQUIRE(utils::ClassUtils::shortenClassName(className, adjusted));
  REQUIRE("o::Test" == adjusted);
  }



  SECTION("MULTIPLE") {
  className = "org::apache::Test";
  adjusted = "";
  REQUIRE(utils::ClassUtils::shortenClassName(className, adjusted));
  REQUIRE("o::a::Test" == adjusted);
  className = "org.apache.Test";
  adjusted = "";
  REQUIRE(utils::ClassUtils::shortenClassName(className, adjusted));
  REQUIRE("o.a.Test" == adjusted);
  className = adjusted;
  adjusted = "";
  REQUIRE(utils::ClassUtils::shortenClassName(className, adjusted));
  REQUIRE("o.a.Test" == adjusted);
  }
}
