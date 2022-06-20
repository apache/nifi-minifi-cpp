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

#include <string>
#include <vector>
#include "utils/ClassUtils.h"
#include "../TestBase.h"
#include "../Catch.h"

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
