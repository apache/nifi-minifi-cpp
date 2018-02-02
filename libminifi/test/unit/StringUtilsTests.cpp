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
#include <cstdlib>
#include "../TestBase.h"
#include "core/Core.h"
#include "utils/StringUtils.h"

using org::apache::nifi::minifi::utils::StringUtils;

TEST_CASE("TestStringUtils::split", "[test split no delimiter]") {
  std::vector<std::string> expected = { "hello" };
  REQUIRE(expected == StringUtils::split("hello", ","));
}

TEST_CASE("TestStringUtils::split2", "[test split single delimiter]") {
  std::vector<std::string> expected = { "hello", "world" };
  REQUIRE(expected == StringUtils::split("hello world", " "));
}

TEST_CASE("TestStringUtils::split3", "[test split multiple delimiter]") {
  std::vector<std::string> expected = { "hello", "world", "I'm", "a", "unit", "test" };
  REQUIRE(expected == StringUtils::split("hello world I'm a unit test", " "));
}

TEST_CASE("TestStringUtils::split4", "[test split classname]") {
  std::vector<std::string> expected = { "org", "apache", "nifi", "minifi", "utils", "StringUtils" };
  REQUIRE(expected == StringUtils::split(org::apache::nifi::minifi::core::getClassName<org::apache::nifi::minifi::utils::StringUtils>(), "::"));
}

TEST_CASE("TestStringUtils::testEnv1", "[test split classname]") {
  std::string test_string = "hello world ${blahblahnamenamenotexist}";

  setenv("blahblahnamenamenotexist", "computer", 0);

  std::string expected = "hello world computer";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv2", "[test split classname]") {
  std::string test_string = "hello world ${blahblahnamenamenotexist";

  setenv("blahblahnamenamenotexist", "computer", 0);

  std::string expected = "hello world ${blahblahnamenamenotexist";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv3", "[test split classname]") {
  std::string test_string = "hello world $${blahblahnamenamenotexist}";

  setenv("blahblahnamenamenotexist", "computer", 0);

  std::string expected = "hello world $computer";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv4", "[test split classname]") {
  std::string test_string = "hello world \\${blahblahnamenamenotexist}";

  setenv("blahblahnamenamenotexist", "computer", 0);

  std::string expected = "hello world ${blahblahnamenamenotexist}";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv5", "[test split classname]") {
  // can't use blahblahnamenamenotexist because the setenv in other functions may have already set it
  std::string test_string = "hello world ${blahblahnamenamenotexist2}";

  std::string expected = "hello world ";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}
