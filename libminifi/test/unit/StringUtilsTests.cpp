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

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include "../TestBase.h"
#include "../Catch.h"
#include "core/Core.h"
#include "utils/StringUtils.h"
#include "utils/Environment.h"

using org::apache::nifi::minifi::utils::StringUtils;
using utils::as_string;

TEST_CASE("StringUtils::chomp works correctly", "[StringUtils][chomp]") {
  using pair_of = std::pair<std::string, std::string>;
  CHECK(StringUtils::chomp("foobar") == pair_of{"foobar", ""});
  CHECK(StringUtils::chomp("foobar\n") == pair_of{"foobar", "\n"});
  CHECK(StringUtils::chomp("foobar\r\n") == pair_of{"foobar", "\r\n"});
  CHECK(StringUtils::chomp("foo\rbar\n") == pair_of{"foo\rbar", "\n"});
}

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

TEST_CASE("TestStringUtils::split5", "[test split with delimiter set to empty string]") {
  std::vector<std::string> expected{ "h", "e", "l", "l", "o", " ", "w", "o", "r", "l", "d" };
  REQUIRE(expected == StringUtils::split("hello world", ""));
}

TEST_CASE("TestStringUtils::split6", "[test split with delimiter with empty results]") {
  std::vector<std::string> expected = {""};
  REQUIRE(expected == StringUtils::split("", ","));
  expected = {"", ""};
  REQUIRE(expected == StringUtils::split(",", ","));
  expected = {"", " ", "", ""};
  REQUIRE(expected == StringUtils::split(", ,,", ","));
}

TEST_CASE("TestStringUtils::splitRemovingEmpty", "[test splitRemovingEmpty multiple delimiter]") {
  std::vector<std::string> expected = { "hello", "world", "I'm", "a", "unit", "test" };
  REQUIRE(expected == StringUtils::split("hello world I'm a unit test", " "));
}

TEST_CASE("TestStringUtils::splitRemovingEmpty2", "[test splitRemovingEmpty no delimiter]") {
  std::vector<std::string> expected = { "hello" };
  REQUIRE(expected == StringUtils::splitRemovingEmpty("hello", ","));
}

TEST_CASE("TestStringUtils::splitRemovingEmpty3", "[test splitRemovingEmpty with delimiter with empty results]") {
  std::vector<std::string> expected = {};
  REQUIRE(expected == StringUtils::splitRemovingEmpty("", ","));
  REQUIRE(expected == StringUtils::splitRemovingEmpty(",", ","));
  expected = {" "};
  REQUIRE(expected == StringUtils::splitRemovingEmpty(", ,,", ","));
}

TEST_CASE("TestStringUtils::splitAndTrim", "[test split with trim with characters]") {
  std::vector<std::string> expected{ "hello", "world peace" };
  REQUIRE(expected == StringUtils::splitAndTrim("hello, world peace", ","));
  expected = {""};
  REQUIRE(expected == StringUtils::splitAndTrim("", ","));
  expected = {"", ""};
  REQUIRE(expected == StringUtils::splitAndTrim(",", ","));
  expected = {"", "", "", ""};
  REQUIRE(expected == StringUtils::splitAndTrim(", ,,", ","));
}

TEST_CASE("StringUtils::splitAndTrim2", "[test split with trim with words]") {
  std::vector<std::string> expected{ "tom", "jerry" };
  REQUIRE(expected == StringUtils::splitAndTrim("tom and jerry", "and"));
  expected = {"", ""};
  REQUIRE(expected == StringUtils::splitAndTrim("and", "and"));
  expected = {"", "", ""};
  REQUIRE(expected == StringUtils::splitAndTrim("andand", "and"));
  expected = {"stan", "pan", ""};
  REQUIRE(expected == StringUtils::splitAndTrim("stan and pan and ", "and"));
  expected = {"", ""};
  REQUIRE(expected == StringUtils::splitAndTrim(" and ", "and"));
  expected = {"a", "b", "c"};
  REQUIRE(expected == StringUtils::splitAndTrim("a and ... b and ...  c", "and ..."));
}

TEST_CASE("StringUtils::splitAndTrimRemovingEmpty", "[test split with trim removing empty strings]") {
  std::vector<std::string> expected{ "tom", "jerry" };
  REQUIRE(expected == StringUtils::splitAndTrimRemovingEmpty("tom and jerry", "and"));
  expected = {};
  REQUIRE(expected == StringUtils::splitAndTrimRemovingEmpty("and", "and"));
  REQUIRE(expected == StringUtils::splitAndTrimRemovingEmpty("andand", "and"));
  expected = {"stan", "pan"};
  REQUIRE(expected == StringUtils::splitAndTrimRemovingEmpty("stan and pan and ", "and"));
  expected = {};
  REQUIRE(expected == StringUtils::splitAndTrimRemovingEmpty(" and ", "and"));
  expected = {"a", "b", "c"};
  REQUIRE(expected == StringUtils::splitAndTrimRemovingEmpty("a and ... b and ...  c", "and ..."));
}

TEST_CASE("StringUtils::replaceEnvironmentVariables works correctly", "[replaceEnvironmentVariables]") {
  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist", "computer", false);

  REQUIRE("hello world computer" == StringUtils::replaceEnvironmentVariables("hello world ${blahblahnamenamenotexist}"));
  REQUIRE("hello world ${blahblahnamenamenotexist" == StringUtils::replaceEnvironmentVariables("hello world ${blahblahnamenamenotexist"));
  REQUIRE("hello world $computer" == StringUtils::replaceEnvironmentVariables("hello world $${blahblahnamenamenotexist}"));
  REQUIRE("hello world ${blahblahnamenamenotexist}" == StringUtils::replaceEnvironmentVariables("hello world \\${blahblahnamenamenotexist}"));
  REQUIRE("the computer costs $123" == StringUtils::replaceEnvironmentVariables("the ${blahblahnamenamenotexist} costs \\$123"));
  REQUIRE("computer bug" == StringUtils::replaceEnvironmentVariables("${blahblahnamenamenotexist} bug"));
  REQUIRE("O computer! My computer!" == StringUtils::replaceEnvironmentVariables("O ${blahblahnamenamenotexist}! My ${blahblahnamenamenotexist}!"));

  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist_2", "no", false);
  REQUIRE("computer says 'no'" == StringUtils::replaceEnvironmentVariables("${blahblahnamenamenotexist} says '${blahblahnamenamenotexist_2}'"));
  REQUIRE("no computer can say no to computer nougats" == StringUtils::replaceEnvironmentVariables(
      "${blahblahnamenamenotexist_2} ${blahblahnamenamenotexist} can say ${blahblahnamenamenotexist_2} to ${blahblahnamenamenotexist} ${blahblahnamenamenotexist_2}ugats"));

  REQUIRE("hello world ${}" == StringUtils::replaceEnvironmentVariables("hello world ${}"));
  REQUIRE("hello world " == StringUtils::replaceEnvironmentVariables("hello world ${blahblahnamenamenotexist_reallydoesnotexist}"));
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

TEST_CASE("TestStringUtils::trim", "[test trim]") {
  REQUIRE("" == StringUtils::trim(""));
  REQUIRE("" == StringUtils::trim(" \n\t"));
  REQUIRE("foobar" == StringUtils::trim("foobar"));
  REQUIRE("foo bar" == StringUtils::trim("foo bar"));
  REQUIRE("foobar" == StringUtils::trim("foobar "));
  REQUIRE("foobar" == StringUtils::trim(" foobar"));
  REQUIRE("foobar" == StringUtils::trim("foobar  "));
  REQUIRE("foobar" == StringUtils::trim("  foobar"));
  REQUIRE("foobar" == StringUtils::trim("  foobar  "));
  REQUIRE("foobar" == StringUtils::trim(" \n\tfoobar\n\t "));

  REQUIRE("" == StringUtils::trimRight(" \n\t"));
  REQUIRE("foobar" == StringUtils::trimRight("foobar"));
  REQUIRE("foo bar" == StringUtils::trimRight("foo bar"));
  REQUIRE("foobar" == StringUtils::trimRight("foobar "));
  REQUIRE(" foobar" == StringUtils::trimRight(" foobar"));
  REQUIRE("foobar" == StringUtils::trimRight("foobar  "));
  REQUIRE("  foobar" == StringUtils::trimRight("  foobar"));
  REQUIRE("  foobar" == StringUtils::trimRight("  foobar  "));
  REQUIRE(" \n\tfoobar" == StringUtils::trimRight(" \n\tfoobar\n\t "));

  REQUIRE("" == StringUtils::trimLeft(" \n\t"));
  REQUIRE("foobar" == StringUtils::trimLeft("foobar"));
  REQUIRE("foo bar" == StringUtils::trimLeft("foo bar"));
  REQUIRE("foobar " == StringUtils::trimLeft("foobar "));
  REQUIRE("foobar" == StringUtils::trimLeft(" foobar"));
  REQUIRE("foobar  " == StringUtils::trimLeft("foobar  "));
  REQUIRE("foobar" == StringUtils::trimLeft("  foobar"));
  REQUIRE("foobar  " == StringUtils::trimLeft("  foobar  "));
  REQUIRE("foobar\n\t " == StringUtils::trimLeft(" \n\tfoobar\n\t "));
}

TEST_CASE("TestStringUtils::startsWith - case sensitive", "[test startsWith]") {
  REQUIRE(StringUtils::startsWith("abcd", ""));
  REQUIRE(StringUtils::startsWith("abcd", "a"));
  REQUIRE(!StringUtils::startsWith("Abcd", "a"));
  REQUIRE(!StringUtils::startsWith("abcd", "A"));
  REQUIRE(StringUtils::startsWith("abcd", "abcd"));
  REQUIRE(StringUtils::startsWith("abcd", "abc"));
  REQUIRE(!StringUtils::startsWith("abcd", "abcde"));

  REQUIRE(StringUtils::startsWith("", ""));
  REQUIRE(!StringUtils::startsWith("", "abcd"));
  REQUIRE(!StringUtils::startsWith("abcd", "b"));
  REQUIRE(!StringUtils::startsWith("abcd", "d"));
}

TEST_CASE("TestStringUtils::endsWith - case sensitive", "[test endsWith]") {
  REQUIRE(StringUtils::endsWith("abcd", ""));
  REQUIRE(StringUtils::endsWith("abcd", "d"));
  REQUIRE(!StringUtils::endsWith("abcD", "d"));
  REQUIRE(!StringUtils::endsWith("abcd", "D"));
  REQUIRE(StringUtils::endsWith("abcd", "abcd"));
  REQUIRE(StringUtils::endsWith("abcd", "bcd"));
  REQUIRE(!StringUtils::endsWith("abcd", "1abcd"));

  REQUIRE(StringUtils::endsWith("", ""));
  REQUIRE(!StringUtils::endsWith("", "abcd"));
  REQUIRE(!StringUtils::endsWith("abcd", "c"));
  REQUIRE(!StringUtils::endsWith("abcd", "a"));
}

TEST_CASE("TestStringUtils::startsWith - case insensitive", "[test startsWith case insensitive]") {
  REQUIRE(StringUtils::startsWith("abcd", "", false));
  REQUIRE(StringUtils::startsWith("abcd", "a", false));
  REQUIRE(StringUtils::startsWith("Abcd", "a", false));
  REQUIRE(StringUtils::startsWith("abcd", "A", false));
  REQUIRE(StringUtils::startsWith("aBcd", "abCd", false));
  REQUIRE(StringUtils::startsWith("abcd", "abc", false));
  REQUIRE(!StringUtils::startsWith("abcd", "abcde", false));

  REQUIRE(StringUtils::startsWith("", "", false));
  REQUIRE(!StringUtils::startsWith("", "abcd", false));
  REQUIRE(!StringUtils::startsWith("abcd", "b", false));
  REQUIRE(!StringUtils::startsWith("abcd", "d", false));
}

TEST_CASE("TestStringUtils::endsWith - case insensitive", "[test endsWith case insensitive]") {
  REQUIRE(StringUtils::endsWith("abcd", "", false));
  REQUIRE(StringUtils::endsWith("abcd", "d", false));
  REQUIRE(StringUtils::endsWith("abcd", "D", false));
  REQUIRE(StringUtils::endsWith("abcD", "d", false));
  REQUIRE(StringUtils::endsWith("abcd", "abcd", false));
  REQUIRE(StringUtils::endsWith("aBcd", "bcD", false));
  REQUIRE(!StringUtils::endsWith("abCd", "1aBcd", false));

  REQUIRE(StringUtils::endsWith("", "", false));
  REQUIRE(!StringUtils::endsWith("", "abcd", false));
  REQUIRE(!StringUtils::endsWith("abcd", "c", false));
  REQUIRE(!StringUtils::endsWith("abcd", "a", false));
}

TEST_CASE("TestStringUtils::toBool", "[test toBool]") {
  std::vector<std::pair<std::string, std::optional<bool>>> cases{
      {"", {}},
      {"true", true},
      {"false", false},
      {" TrUe   ", true},
      {"\n \r FaLsE \t", false},
      {"not false", {}}
  };
  for (const auto& test_case : cases) {
    REQUIRE(StringUtils::toBool(test_case.first) == test_case.second);
  }
}

TEST_CASE("TestStringUtils::testHexEncode", "[test hex encode]") {
  REQUIRE("" == StringUtils::to_hex(""));
  REQUIRE("6f" == StringUtils::to_hex("o"));
  REQUIRE("666f6f626172" == StringUtils::to_hex("foobar"));
  REQUIRE("000102030405060708090a0b0c0d0e0f" == StringUtils::to_hex(gsl::make_span(std::vector<uint8_t>{
      0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f}).as_span<const std::byte>()));
  REQUIRE("6F" == StringUtils::to_hex("o", true /*uppercase*/));
  REQUIRE("666F6F626172" == StringUtils::to_hex("foobar", true /*uppercase*/));
  REQUIRE("000102030405060708090A0B0C0D0E0F" == StringUtils::to_hex(gsl::make_span(std::vector<uint8_t>{
      0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f}).as_span<const std::byte>(), true /*uppercase*/));
}

TEST_CASE("TestStringUtils::testHexDecode", "[test hex decode]") {
  REQUIRE(StringUtils::from_hex("").empty());
  REQUIRE("o" == StringUtils::from_hex("6f", utils::as_string));
  REQUIRE("o" == StringUtils::from_hex("6F", utils::as_string));
  REQUIRE("foobar" == StringUtils::from_hex("666f6f626172", utils::as_string));
  REQUIRE("foobar" == StringUtils::from_hex("666F6F626172", utils::as_string));
  REQUIRE("foobar" == StringUtils::from_hex("66:6F:6F:62:61:72", utils::as_string));
  REQUIRE("foobar" == StringUtils::from_hex("66 6F 6F 62 61 72", utils::as_string));
  REQUIRE(std::string({0x00, 0x01, 0x02, 0x03,
                       0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b,
                       0x0c, 0x0d, 0x0e, 0x0f}) == StringUtils::from_hex("000102030405060708090a0b0c0d0e0f", utils::as_string));
  REQUIRE(std::string({0x00, 0x01, 0x02, 0x03,
                       0x04, 0x05, 0x06, 0x07,
                       0x08, 0x09, 0x0a, 0x0b,
                       0x0c, 0x0d, 0x0e, 0x0f}) == StringUtils::from_hex("000102030405060708090A0B0C0D0E0F", utils::as_string));

  REQUIRE_THROWS_WITH(StringUtils::from_hex("666f6f62617"), "Hexencoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_hex("666f6f6261 7"), "Hexencoded string is malformed");
}

TEST_CASE("TestStringUtils::testHexEncodeDecode", "[test hex encode decode]") {
  std::mt19937 gen(std::random_device { }());
  for (size_t i = 0U; i < 1024U; i++) {
    const bool uppercase = gen() % 2;
    const size_t length = gen() % 1024;
    std::vector<std::byte> data(length);
    std::generate_n(data.begin(), data.size(), [&]() -> std::byte {
      return static_cast<std::byte>(gen() % 256);
    });
    auto hex = utils::StringUtils::to_hex(data, uppercase);
    REQUIRE(data == utils::StringUtils::from_hex(hex));
  }
}

TEST_CASE("TestStringUtils::testBase64Encode", "[test base64 encode]") {
  REQUIRE("" == StringUtils::to_base64(""));

  REQUIRE("bw==" == StringUtils::to_base64("o"));
  REQUIRE("b28=" == StringUtils::to_base64("oo"));
  REQUIRE("b29v" == StringUtils::to_base64("ooo"));
  REQUIRE("b29vbw==" == StringUtils::to_base64("oooo"));
  REQUIRE("b29vb28=" == StringUtils::to_base64("ooooo"));
  REQUIRE("b29vb29v" == StringUtils::to_base64("oooooo"));

  REQUIRE("bw" == StringUtils::to_base64("o", false /*url*/, false /*padded*/));
  REQUIRE("b28" == StringUtils::to_base64("oo", false /*url*/, false /*padded*/));
  REQUIRE("b29v" == StringUtils::to_base64("ooo", false /*url*/, false /*padded*/));
  REQUIRE("b29vbw" == StringUtils::to_base64("oooo", false /*url*/, false /*padded*/));
  REQUIRE("b29vb28" == StringUtils::to_base64("ooooo", false /*url*/, false /*padded*/));
  REQUIRE("b29vb29v" == StringUtils::to_base64("oooooo", false /*url*/, false /*padded*/));

  REQUIRE("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ==
      StringUtils::to_base64(gsl::make_span(std::vector<uint8_t>{
          0x00, 0x10, 0x83, 0x10,
          0x51, 0x87, 0x20, 0x92,
          0x8b, 0x30, 0xd3, 0x8f,
          0x41, 0x14, 0x93, 0x51,
          0x55, 0x97, 0x61, 0x96,
          0x9b, 0x71, 0xd7, 0x9f,
          0x82, 0x18, 0xa3, 0x92,
          0x59, 0xa7, 0xa2, 0x9a,
          0xab, 0xb2, 0xdb, 0xaf,
          0xc3, 0x1c, 0xb3, 0xd3,
          0x5d, 0xb7, 0xe3, 0x9e,
          0xbb, 0xf3, 0xdf, 0xbf}).as_span<const std::byte>()));
  REQUIRE("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" ==
      StringUtils::to_base64(gsl::make_span(std::vector<uint8_t>{
          0x00, 0x10, 0x83, 0x10,
          0x51, 0x87, 0x20, 0x92,
          0x8b, 0x30, 0xd3, 0x8f,
          0x41, 0x14, 0x93, 0x51,
          0x55, 0x97, 0x61, 0x96,
          0x9b, 0x71, 0xd7, 0x9f,
          0x82, 0x18, 0xa3, 0x92,
          0x59, 0xa7, 0xa2, 0x9a,
          0xab, 0xb2, 0xdb, 0xaf,
          0xc3, 0x1c, 0xb3, 0xd3,
          0x5d, 0xb7, 0xe3, 0x9e,
          0xbb, 0xf3, 0xdf, 0xbf}).as_span<const std::byte>(), true /*url*/));
}

TEST_CASE("TestStringUtils::testBase64Decode", "[test base64 decode]") {
  REQUIRE(StringUtils::from_base64("", as_string).empty());
  REQUIRE("o" == StringUtils::from_base64("bw==", as_string));
  REQUIRE("oo" == StringUtils::from_base64("b28=", as_string));
  REQUIRE("ooo" == StringUtils::from_base64("b29v", as_string));
  REQUIRE("oooo" == StringUtils::from_base64("b29vbw==", as_string));
  REQUIRE("ooooo" == StringUtils::from_base64("b29vb28=", as_string));
  REQUIRE("oooooo" == StringUtils::from_base64("b29vb29v", as_string));
  REQUIRE("\xfb\xff\xbf" == StringUtils::from_base64("-_-_", as_string));
  REQUIRE("\xfb\xff\xbf" == StringUtils::from_base64("+/+/", as_string));
  REQUIRE(std::string({   0,   16, -125,   16,
                         81, -121,   32, -110,
                       -117,   48,  -45, -113,
                         65,   20, -109,   81,
                         85, -105,   97, -106,
                       -101,  113,  -41,  -97,
                       -126,   24,  -93, -110,
                         89,  -89,  -94, -102,
                        -85,  -78,  -37,  -81,
                        -61,   28,  -77,  -45,
                         93,  -73,  -29,  -98,
                        -69,  -13,  -33,  -65}) == StringUtils::from_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", as_string));
  REQUIRE(std::string({   0,   16, -125,   16,
                         81, -121,   32, -110,
                       -117,   48,  -45, -113,
                         65,   20, -109,   81,
                         85, -105,   97, -106,
                       -101,  113,  -41,  -97,
                       -126,   24,  -93, -110,
                         89,  -89,  -94, -102,
                        -85,  -78,  -37,  -81,
                        -61,   28,  -77,  -45,
                         93,  -73,  -29,  -98,
                        -69,  -13,  -33,  -65}) == StringUtils::from_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", as_string));

  REQUIRE("foobarbuzz" == StringUtils::from_base64("Zm9vYmFyYnV6eg==", as_string));
  REQUIRE("foobarbuzz"== StringUtils::from_base64("\r\nZm9vYmFyYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == StringUtils::from_base64("Zm9\r\nvYmFyYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == StringUtils::from_base64("Zm\r9vYmFy\n\n\n\n\n\n\n\nYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == StringUtils::from_base64("\nZ\nm\n9\nv\nY\nm\nF\ny\nY\nn\nV\n6\ne\ng\n=\n=\n", as_string));

  REQUIRE_THROWS_WITH(StringUtils::from_base64("a"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aaaaa"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa="), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aaaaaa="), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa==?"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa==a"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa==="), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("?"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aaaa?"), "Base64 encoded string is malformed");
}

TEST_CASE("TestStringUtils::testBase64EncodeDecode", "[test base64 encode decode]") {
  std::mt19937 gen(std::random_device { }());
  for (size_t i = 0U; i < 1024U; i++) {
    const bool url = gen() % 2;
    const bool padded = gen() % 2;
    const size_t length = gen() % 1024;
    std::vector<std::byte> data(length);
    std::generate_n(data.begin(), data.size(), [&]() -> std::byte {
      return static_cast<std::byte>(gen() % 256);
    });
    auto base64 = utils::StringUtils::to_base64(data, url, padded);
    REQUIRE(data == utils::StringUtils::from_base64(base64));
  }
}

TEST_CASE("TestStringUtils::testJoinPack", "[test join_pack]") {
  std::string stdstr = "std::string";
  const char* cstr = "c string";
  const char carr[] = "char array";
  REQUIRE(utils::StringUtils::join_pack("rvalue c string, ", cstr, std::string{ ", rval std::string, " }, stdstr, ", ", carr)
              == "rvalue c string, c string, rval std::string, std::string, char array");
}

TEST_CASE("TestStringUtils::testJoinPackWstring", "[test join_pack wstring]") {
  std::wstring stdstr = L"std::string";
  const wchar_t* cstr = L"c string";
  const wchar_t carr[] = L"char array";
  REQUIRE(utils::StringUtils::join_pack(L"rvalue c string, ", cstr, std::wstring{ L", rval std::string, " }, stdstr, L", ", carr)
              == L"rvalue c string, c string, rval std::string, std::string, char array");
}

/* doesn't and shouldn't compile
TEST_CASE("TestStringUtils::testJoinPackNegative", "[test join_pack negative]") {
  std::wstring stdstr = L"std::string";
  const wchar_t* cstr = L"c string";
  const wchar_t carr[] = L"char array";
  REQUIRE(utils::StringUtils::join_pack("rvalue c string, ", cstr, std::string{ ", rval std::string, " }, stdstr, ", ", carr)
              == "rvalue c string, c string, rval std::string, std::string, char array");
}
 */

TEST_CASE("StringUtils::replaceOne works correctly", "[replaceOne]") {
  REQUIRE(utils::StringUtils::replaceOne("", "x", "y") == "");
  REQUIRE(utils::StringUtils::replaceOne("banana", "a", "_") == "b_nana");
  REQUIRE(utils::StringUtils::replaceOne("banana", "b", "_") == "_anana");
  REQUIRE(utils::StringUtils::replaceOne("banana", "x", "y") == "banana");
  REQUIRE(utils::StringUtils::replaceOne("banana", "an", "") == "bana");
  REQUIRE(utils::StringUtils::replaceOne("banana", "an", "AN") == "bANana");
  REQUIRE(utils::StringUtils::replaceOne("banana", "an", "***") == "b***ana");
  REQUIRE(utils::StringUtils::replaceOne("banana", "banana", "kiwi") == "kiwi");
  REQUIRE(utils::StringUtils::replaceOne("banana", "banana", "grapefruit") == "grapefruit");
  REQUIRE(utils::StringUtils::replaceOne("fruit", "", "grape") == "grapefruit");
}

TEST_CASE("StringUtils::replaceAll works correctly", "[replaceAll]") {
  auto replaceAll = [](std::string input, const std::string &from, const std::string &to) -> std::string {
    return utils::StringUtils::replaceAll(input, from, to);
  };
  REQUIRE(replaceAll("", "x", "y") == "");
  REQUIRE(replaceAll("banana", "a", "_") == "b_n_n_");
  REQUIRE(replaceAll("banana", "b", "_") == "_anana");
  REQUIRE(replaceAll("banana", "x", "y") == "banana");
  REQUIRE(replaceAll("banana", "an", "") == "ba");
  REQUIRE(replaceAll("banana", "an", "AN") == "bANANa");
  REQUIRE(replaceAll("banana", "an", "***") == "b******a");
  REQUIRE(replaceAll("banana", "banana", "kiwi") == "kiwi");
  REQUIRE(replaceAll("banana", "banana", "grapefruit") == "grapefruit");
  REQUIRE(replaceAll("abc", "", "d") == "dadbdcd");
  REQUIRE(replaceAll("banana", "", "") == "banana");
}

TEST_CASE("StringUtils::countOccurrences works correctly", "[countOccurrences]") {
  REQUIRE(utils::StringUtils::countOccurrences("", "a") == std::make_pair(size_t{0}, 0));
  REQUIRE(utils::StringUtils::countOccurrences("abc", "a") == std::make_pair(size_t{0}, 1));
  REQUIRE(utils::StringUtils::countOccurrences("abc", "b") == std::make_pair(size_t{1}, 1));
  REQUIRE(utils::StringUtils::countOccurrences("abc", "x") == std::make_pair(size_t{0}, 0));
  REQUIRE(utils::StringUtils::countOccurrences("banana", "a") == std::make_pair(size_t{5}, 3));
  REQUIRE(utils::StringUtils::countOccurrences("banana", "an") == std::make_pair(size_t{3}, 2));
  REQUIRE(utils::StringUtils::countOccurrences("aaaaaaaa", "aaa") == std::make_pair(size_t{3}, 2));  // overlapping occurrences are not counted
  REQUIRE(utils::StringUtils::countOccurrences("abc", "") == std::make_pair(size_t{3}, 4));  // "" occurs at the start, between chars, and at the end
  REQUIRE(utils::StringUtils::countOccurrences("", "") == std::make_pair(size_t{0}, 1));
}

TEST_CASE("StringUtils::removeFramingCharacters works correctly", "[removeFramingCharacters]") {
  REQUIRE(utils::StringUtils::removeFramingCharacters("", 'a') == "");
  REQUIRE(utils::StringUtils::removeFramingCharacters("a", 'a') == "a");
  REQUIRE(utils::StringUtils::removeFramingCharacters("aa", 'a') == "");
  REQUIRE(utils::StringUtils::removeFramingCharacters("\"abba\"", '"') == "abba");
  REQUIRE(utils::StringUtils::removeFramingCharacters("\"\"abba\"\"", '"') == "\"abba\"");
}

// ignore terminating \0 character
template<size_t N>
gsl::span<const std::byte> from_cstring(const char (&str)[N]) {
  return gsl::span<const char>(str, N-1).as_span<const std::byte>();
}

TEST_CASE("StringUtils::escapeUnprintableBytes", "[escapeUnprintableBytes]") {
  REQUIRE(StringUtils::escapeUnprintableBytes(from_cstring("abcd")) == "abcd");
  REQUIRE(StringUtils::escapeUnprintableBytes(from_cstring("ab\n\r\t\v\fde")) == "ab\\n\\r\\t\\v\\fde");
  REQUIRE(StringUtils::escapeUnprintableBytes(from_cstring("ab\x00""c\x01""d")) == "ab\\x00c\\x01d");
}

TEST_CASE("StringUtils::matchesSequence works correctly", "[matchesSequence]") {
  REQUIRE(StringUtils::matchesSequence("abcdef", {"abc", "def"}));
  REQUIRE(!StringUtils::matchesSequence("abcef", {"abc", "def"}));
  REQUIRE(StringUtils::matchesSequence("xxxabcxxxdefxxx", {"abc", "def"}));
  REQUIRE(!StringUtils::matchesSequence("defabc", {"abc", "def"}));
  REQUIRE(StringUtils::matchesSequence("xxxabcxxxabcxxxdefxxx", {"abc", "def"}));
  REQUIRE(StringUtils::matchesSequence("xxxabcxxxabcxxxdefxxx", {"abc", "abc", "def"}));
  REQUIRE(!StringUtils::matchesSequence("xxxabcxxxdefxxx", {"abc", "abc", "def"}));
}
