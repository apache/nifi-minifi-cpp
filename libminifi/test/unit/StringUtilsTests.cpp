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
#include <list>
#include <vector>
#include <cstdlib>
#include <random>
#include <algorithm>
#include <cstdint>
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

TEST_CASE("TestStringUtils::testJoin", "[test string join]") {
  std::set<std::string> strings = {"3", "2", "1"};
  REQUIRE(StringUtils::join(",", strings) == "1,2,3");

  std::wstring sep = L"é";
  std::vector<std::wstring> wstrings = {L"1", L"2"};
  REQUIRE(StringUtils::join(sep, wstrings) == L"1é2");

  std::list<uint64_t> ulist = {1, 2};
  REQUIRE(StringUtils::join(sep, ulist) == L"1é2");

  REQUIRE(StringUtils::join(">", ulist) == "1>2");

  REQUIRE(StringUtils::join("", ulist) == "12");

  REQUIRE(StringUtils::join("this separator wont appear", std::vector<std::string>()) == "");
}

TEST_CASE("TestStringUtils::testHexEncode", "[test hex encode]") {
  REQUIRE("" == StringUtils::to_hex(""));
  REQUIRE("6f" == StringUtils::to_hex("o"));
  REQUIRE("666f6f626172" == StringUtils::to_hex("foobar"));
  REQUIRE("000102030405060708090a0b0c0d0e0f" == StringUtils::to_hex({0x00, 0x01, 0x02, 0x03,
                                                                     0x04, 0x05, 0x06, 0x07,
                                                                     0x08, 0x09, 0x0a, 0x0b,
                                                                     0x0c, 0x0d, 0x0e, 0x0f}));
  REQUIRE("6F" == StringUtils::to_hex("o", true /*uppercase*/));
  REQUIRE("666F6F626172" == StringUtils::to_hex("foobar", true /*uppercase*/));
  REQUIRE("000102030405060708090A0B0C0D0E0F" == StringUtils::to_hex({0x00, 0x01, 0x02, 0x03,
                                                                     0x04, 0x05, 0x06, 0x07,
                                                                     0x08, 0x09, 0x0a, 0x0b,
                                                                     0x0c, 0x0d, 0x0e, 0x0f}, true /*uppercase*/));
}

TEST_CASE("TestStringUtils::testHexDecode", "[test hex decode]") {
  REQUIRE("" == StringUtils::from_hex(""));
  REQUIRE("o" == StringUtils::from_hex("6f"));
  REQUIRE("o" == StringUtils::from_hex("6F"));
  REQUIRE("foobar" == StringUtils::from_hex("666f6f626172"));
  REQUIRE("foobar" == StringUtils::from_hex("666F6F626172"));
  REQUIRE("foobar" == StringUtils::from_hex("66:6F:6F:62:61:72"));
  REQUIRE("foobar" == StringUtils::from_hex("66 6F 6F 62 61 72"));
  REQUIRE(std::string({0x00, 0x01, 0x02, 0x03,
                       0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b,
                       0x0c, 0x0d, 0x0e, 0x0f}) == StringUtils::from_hex("000102030405060708090a0b0c0d0e0f"));
  REQUIRE(std::string({0x00, 0x01, 0x02, 0x03,
                       0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b,
                       0x0c, 0x0d, 0x0e, 0x0f}) == StringUtils::from_hex("000102030405060708090A0B0C0D0E0F"));
  try {
    StringUtils::from_hex("666f6f62617");
    abort();
  } catch (std::exception& e) {
    REQUIRE(std::string("Hexencoded string is malformatted") == e.what());
  }
  try {
    StringUtils::from_hex("666f6f6261 7");
    abort();
  } catch (std::exception& e) {
    REQUIRE(std::string("Hexencoded string is malformatted") == e.what());
  }
}

TEST_CASE("TestStringUtils::testHexEncodeDecode", "[test hex encode decode]") {
  std::mt19937 gen(std::random_device { }());
  const bool uppercase = gen() % 2;
  const size_t length = gen() % 1024;
  std::vector<uint8_t> data(length);
  std::generate_n(data.begin(), data.size(), [&]() -> uint8_t {
    return gen() % 256;
  });
  auto hex = utils::StringUtils::to_hex(data.data(), data.size(), uppercase);
  REQUIRE(data == utils::StringUtils::from_hex(hex.data(), hex.size()));
}
