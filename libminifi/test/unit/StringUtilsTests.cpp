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

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <string>
#include <vector>

#include "../TestBase.h"
#include "core/Core.h"
#include "utils/StringUtils.h"

TEST_CASE("TestStringUtils::starts_with", "[test starts with string shorter than prefix]") {
  REQUIRE(false == org::apache::nifi::minifi::utils::StringUtils::starts_with("hello", "hello."));
}

TEST_CASE("TestStringUtils::starts_with2", "[test starts with true]") {
  REQUIRE(true == org::apache::nifi::minifi::utils::StringUtils::starts_with("hello.", "hello"));
}

TEST_CASE("TestStringUtils::starts_with3", "[test starts with false]") {
  REQUIRE(false == org::apache::nifi::minifi::utils::StringUtils::starts_with("hello world", "hello."));
}

TEST_CASE("TestStringUtils::split", "[test split no delimiter]") {
  std::vector<std::string> expected = {"hello"};
  REQUIRE(expected == org::apache::nifi::minifi::utils::StringUtils::split("hello", ","));
}

TEST_CASE("TestStringUtils::split2", "[test split single delimiter]") {
  std::vector<std::string> expected = {"hello", "world"};
  REQUIRE(expected == org::apache::nifi::minifi::utils::StringUtils::split("hello world", " "));
}

TEST_CASE("TestStringUtils::split3", "[test split multiple delimiter]") {
  std::vector<std::string> expected = {"hello", "world", "I'm", "a", "unit", "test"};
  REQUIRE(expected == org::apache::nifi::minifi::utils::StringUtils::split("hello world I'm a unit test", " "));
}

TEST_CASE("TestStringUtils::split4", "[test split classname]") {
  std::vector<std::string> expected = {"org", "apache", "nifi", "minifi", "utils", "StringUtils"};
  REQUIRE(expected == org::apache::nifi::minifi::utils::StringUtils::split(
    org::apache::nifi::minifi::core::getClassName<org::apache::nifi::minifi::utils::StringUtils>(), "::"));
}
