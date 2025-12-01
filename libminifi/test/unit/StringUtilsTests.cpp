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

#include <cstdint>
#include <list>
#include <optional>
#include <random>
#include <string>
#include <vector>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "utils/StringUtils.h"
#include "utils/Environment.h"

#ifdef WIN32
#include "utils/UnicodeConversion.h"
#endif

namespace org::apache::nifi::minifi::utils {

// NOLINTBEGIN(readability-container-size-empty)

TEST_CASE("string::chomp works correctly", "[chomp]") {
  using pair_of = std::pair<std::string, std::string>;
  CHECK(string::chomp("foobar") == pair_of{"foobar", ""});
  CHECK(string::chomp("foobar\n") == pair_of{"foobar", "\n"});
  CHECK(string::chomp("foobar\r\n") == pair_of{"foobar", "\r\n"});
  CHECK(string::chomp("foo\rbar\n") == pair_of{"foo\rbar", "\n"});
}

TEST_CASE("test string::split", "[test split no delimiter]") {
  std::vector<std::string> expected = {"hello"};
  REQUIRE(expected == string::split("hello", ","));
}

TEST_CASE("test string::split2", "[test split single delimiter]") {
  std::vector<std::string> expected = {"hello", "world"};
  REQUIRE(expected == string::split("hello world", " "));
}

TEST_CASE("test string::split3", "[test split multiple delimiter]") {
  std::vector<std::string> expected = {"hello", "world", "I'm", "a", "unit", "test"};
  REQUIRE(expected == string::split("hello world I'm a unit test", " "));
}

class Foo {};

TEST_CASE("test string::split4", "[test split classname]") {
  std::vector<std::string> expected = {"org", "apache", "nifi", "minifi", "utils", "Foo"};
  REQUIRE(expected == string::split(org::apache::nifi::minifi::core::className<Foo>(), "::"));
}

TEST_CASE("test string::split5", "[test split with delimiter set to empty string]") {
  std::vector<std::string> expected{"h", "e", "l", "l", "o", " ", "w", "o", "r", "l", "d"};
  REQUIRE(expected == string::split("hello world", ""));
}

TEST_CASE("test string::split6", "[test split with delimiter with empty results]") {
  std::vector<std::string> expected = {""};
  REQUIRE(expected == string::split("", ","));
  expected = {"", ""};
  REQUIRE(expected == string::split(",", ","));
  expected = {"", " ", "", ""};
  REQUIRE(expected == string::split(", ,,", ","));
}

TEST_CASE("test string::splitRemovingEmpty", "[test splitRemovingEmpty multiple delimiter]") {
  std::vector<std::string> expected = {"hello", "world", "I'm", "a", "unit", "test"};
  REQUIRE(expected == string::split("hello world I'm a unit test", " "));
}

TEST_CASE("test string::splitRemovingEmpty2", "[test splitRemovingEmpty no delimiter]") {
  std::vector<std::string> expected = {"hello"};
  REQUIRE(expected == string::splitRemovingEmpty("hello", ","));
}

TEST_CASE("test string::splitRemovingEmpty3", "[test splitRemovingEmpty with delimiter with empty results]") {
  std::vector<std::string> expected = {};
  REQUIRE(expected == string::splitRemovingEmpty("", ","));
  REQUIRE(expected == string::splitRemovingEmpty(",", ","));
  expected = {" "};
  REQUIRE(expected == string::splitRemovingEmpty(", ,,", ","));
}

TEST_CASE("test string::splitAndTrim", "[test split with trim with characters]") {
  std::vector<std::string> expected{"hello", "world peace"};
  REQUIRE(expected == string::splitAndTrim("hello, world peace", ","));
  expected = {""};
  REQUIRE(expected == string::splitAndTrim("", ","));
  expected = {"", ""};
  REQUIRE(expected == string::splitAndTrim(",", ","));
  expected = {"", "", "", ""};
  REQUIRE(expected == string::splitAndTrim(", ,,", ","));
}

TEST_CASE("string::splitAndTrim2", "[test split with trim with words]") {
  std::vector<std::string> expected{"tom", "jerry"};
  REQUIRE(expected == string::splitAndTrim("tom and jerry", "and"));
  expected = {"", ""};
  REQUIRE(expected == string::splitAndTrim("and", "and"));
  expected = {"", "", ""};
  REQUIRE(expected == string::splitAndTrim("andand", "and"));
  expected = {"stan", "pan", ""};
  REQUIRE(expected == string::splitAndTrim("stan and pan and ", "and"));
  expected = {"", ""};
  REQUIRE(expected == string::splitAndTrim(" and ", "and"));
  expected = {"a", "b", "c"};
  REQUIRE(expected == string::splitAndTrim("a and ... b and ...  c", "and ..."));
}

TEST_CASE("string::splitAndTrimRemovingEmpty", "[test split with trim removing empty strings]") {
  std::vector<std::string> expected{"tom", "jerry"};
  REQUIRE(expected == string::splitAndTrimRemovingEmpty("tom and jerry", "and"));
  expected = {};
  REQUIRE(expected == string::splitAndTrimRemovingEmpty("and", "and"));
  REQUIRE(expected == string::splitAndTrimRemovingEmpty("andand", "and"));
  expected = {"stan", "pan"};
  REQUIRE(expected == string::splitAndTrimRemovingEmpty("stan and pan and ", "and"));
  expected = {};
  REQUIRE(expected == string::splitAndTrimRemovingEmpty(" and ", "and"));
  expected = {"a", "b", "c"};
  REQUIRE(expected == string::splitAndTrimRemovingEmpty("a and ... b and ...  c", "and ..."));
}

TEST_CASE("string::replaceEnvironmentVariables works correctly", "[replaceEnvironmentVariables]") {
  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist", "computer", false);

  REQUIRE("hello world computer" == string::replaceEnvironmentVariables("hello world ${blahblahnamenamenotexist}"));
  REQUIRE("hello world ${blahblahnamenamenotexist" == string::replaceEnvironmentVariables("hello world ${blahblahnamenamenotexist"));
  REQUIRE("hello world $computer" == string::replaceEnvironmentVariables("hello world $${blahblahnamenamenotexist}"));
  REQUIRE("hello world ${blahblahnamenamenotexist}" == string::replaceEnvironmentVariables("hello world \\${blahblahnamenamenotexist}"));
  REQUIRE("the computer costs $123" == string::replaceEnvironmentVariables("the ${blahblahnamenamenotexist} costs \\$123"));
  REQUIRE("computer bug" == string::replaceEnvironmentVariables("${blahblahnamenamenotexist} bug"));
  REQUIRE("O computer! My computer!" == string::replaceEnvironmentVariables("O ${blahblahnamenamenotexist}! My ${blahblahnamenamenotexist}!"));

  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist_2", "no", false);
  REQUIRE("computer says 'no'" == string::replaceEnvironmentVariables("${blahblahnamenamenotexist} says '${blahblahnamenamenotexist_2}'"));
  REQUIRE("no computer can say no to computer nougats" == string::replaceEnvironmentVariables(
      "${blahblahnamenamenotexist_2} ${blahblahnamenamenotexist} can say ${blahblahnamenamenotexist_2} to ${blahblahnamenamenotexist} ${blahblahnamenamenotexist_2}ugats"));

  REQUIRE("hello world ${}" == string::replaceEnvironmentVariables("hello world ${}"));
  REQUIRE("hello world " == string::replaceEnvironmentVariables("hello world ${blahblahnamenamenotexist_reallydoesnotexist}"));
}

TEST_CASE("test string::testJoin", "[test string join]") {
  std::set<std::string> strings = {"3", "2", "1"};
  CHECK(string::join(",", strings) == "1,2,3");

  std::wstring sep = L"é";
  std::vector<std::wstring> wstrings = {L"1", L"2"};
  CHECK(string::join(sep, wstrings) == L"1é2");

  std::list<uint64_t> ulist = {1, 2};
  CHECK(string::join(sep, ulist) == L"1é2");

  CHECK(string::join(">", ulist) == "1>2");

  CHECK(string::join("", ulist) == "12");

  CHECK(string::join("this separator wont appear", std::vector<std::string>()) == "");
}

TEST_CASE("Test the join function with a projection", "[join][projection]") {
  std::vector<std::string> fruits = {"APPLE", "OrAnGe"};
  CHECK(string::join(", ", fruits, [](const auto& fruit) { return string::toLower(fruit); }) == "apple, orange");

  std::set numbers_set = {3, 2, 1};
  CHECK(string::join(", ", numbers_set, [](auto x) { return std::to_string(x); }) == "1, 2, 3");

  std::wstring sep = L"é";
  std::vector numbers_vector = {1, 2};
  CHECK(string::join(sep, numbers_vector, [](auto x) { return std::to_wstring(x); }) == L"1é2");

  std::list<uint64_t> numbers_list = {1, 2, 3};
  CHECK(string::join(", ", numbers_list, [](auto x) { return std::to_string(x * x); }) == "1, 4, 9");

  CHECK(string::join(std::string_view{"-"}, numbers_list, [](auto x) { return std::to_string(5 * x); }) == "5-10-15");

  CHECK(string::join(std::string{}, numbers_list, [](auto x) { return '<' + std::to_string(x) + '>'; }) == "<1><2><3>");

  CHECK(string::join("this separator wont appear", std::vector<int>{}, [](int) { return std::string_view{"foo"}; }) == "");
}

TEST_CASE("test string::trim", "[test trim]") {
  REQUIRE("" == string::trim(""));
  REQUIRE("" == string::trim(" \n\t"));
  REQUIRE("foobar" == string::trim("foobar"));
  REQUIRE("foo bar" == string::trim("foo bar"));
  REQUIRE("foobar" == string::trim("foobar "));
  REQUIRE("foobar" == string::trim(" foobar"));
  REQUIRE("foobar" == string::trim("foobar  "));
  REQUIRE("foobar" == string::trim("  foobar"));
  REQUIRE("foobar" == string::trim("  foobar  "));
  REQUIRE("foobar" == string::trim(" \n\tfoobar\n\t "));

  REQUIRE("" == string::trimRight(" \n\t"));
  REQUIRE("foobar" == string::trimRight("foobar"));
  REQUIRE("foo bar" == string::trimRight("foo bar"));
  REQUIRE("foobar" == string::trimRight("foobar "));
  REQUIRE(" foobar" == string::trimRight(" foobar"));
  REQUIRE("foobar" == string::trimRight("foobar  "));
  REQUIRE("  foobar" == string::trimRight("  foobar"));
  REQUIRE("  foobar" == string::trimRight("  foobar  "));
  REQUIRE(" \n\tfoobar" == string::trimRight(" \n\tfoobar\n\t "));

  REQUIRE("" == string::trimLeft(" \n\t"));
  REQUIRE("foobar" == string::trimLeft("foobar"));
  REQUIRE("foo bar" == string::trimLeft("foo bar"));
  REQUIRE("foobar " == string::trimLeft("foobar "));
  REQUIRE("foobar" == string::trimLeft(" foobar"));
  REQUIRE("foobar  " == string::trimLeft("foobar  "));
  REQUIRE("foobar" == string::trimLeft("  foobar"));
  REQUIRE("foobar  " == string::trimLeft("  foobar  "));
  REQUIRE("foobar\n\t " == string::trimLeft(" \n\tfoobar\n\t "));
}

TEST_CASE("test string::startsWith - case sensitive", "[test startsWith]") {
  REQUIRE(string::startsWith("abcd", ""));
  REQUIRE(string::startsWith("abcd", "a"));
  REQUIRE(!string::startsWith("Abcd", "a"));
  REQUIRE(!string::startsWith("abcd", "A"));
  REQUIRE(string::startsWith("abcd", "abcd"));
  REQUIRE(string::startsWith("abcd", "abc"));
  REQUIRE(!string::startsWith("abcd", "abcde"));

  REQUIRE(string::startsWith("", ""));
  REQUIRE(!string::startsWith("", "abcd"));
  REQUIRE(!string::startsWith("abcd", "b"));
  REQUIRE(!string::startsWith("abcd", "d"));
}

TEST_CASE("test string::endsWith - case sensitive", "[test endsWith]") {
  REQUIRE(string::endsWith("abcd", ""));
  REQUIRE(string::endsWith("abcd", "d"));
  REQUIRE(!string::endsWith("abcD", "d"));
  REQUIRE(!string::endsWith("abcd", "D"));
  REQUIRE(string::endsWith("abcd", "abcd"));
  REQUIRE(string::endsWith("abcd", "bcd"));
  REQUIRE(!string::endsWith("abcd", "1abcd"));

  REQUIRE(string::endsWith("", ""));
  REQUIRE(!string::endsWith("", "abcd"));
  REQUIRE(!string::endsWith("abcd", "c"));
  REQUIRE(!string::endsWith("abcd", "a"));
}

TEST_CASE("test string::startsWith - case insensitive", "[test startsWith case insensitive]") {
  REQUIRE(string::startsWith("abcd", "", false));
  REQUIRE(string::startsWith("abcd", "a", false));
  REQUIRE(string::startsWith("Abcd", "a", false));
  REQUIRE(string::startsWith("abcd", "A", false));
  REQUIRE(string::startsWith("aBcd", "abCd", false));
  REQUIRE(string::startsWith("abcd", "abc", false));
  REQUIRE(!string::startsWith("abcd", "abcde", false));

  REQUIRE(string::startsWith("", "", false));
  REQUIRE(!string::startsWith("", "abcd", false));
  REQUIRE(!string::startsWith("abcd", "b", false));
  REQUIRE(!string::startsWith("abcd", "d", false));
}

TEST_CASE("test string::endsWith - case insensitive", "[test endsWith case insensitive]") {
  REQUIRE(string::endsWith("abcd", "", false));
  REQUIRE(string::endsWith("abcd", "d", false));
  REQUIRE(string::endsWith("abcd", "D", false));
  REQUIRE(string::endsWith("abcD", "d", false));
  REQUIRE(string::endsWith("abcd", "abcd", false));
  REQUIRE(string::endsWith("aBcd", "bcD", false));
  REQUIRE(!string::endsWith("abCd", "1aBcd", false));

  REQUIRE(string::endsWith("", "", false));
  REQUIRE(!string::endsWith("", "abcd", false));
  REQUIRE(!string::endsWith("abcd", "c", false));
  REQUIRE(!string::endsWith("abcd", "a", false));
}

TEST_CASE("test string::toBool", "[test toBool]") {
  std::vector<std::pair<std::string, std::optional<bool>>> cases{
      {"", {}},
      {"true", true},
      {"false", false},
      {" TrUe   ", true},
      {"\n \r FaLsE \t", false},
      {"not false", {}}
  };
  for (const auto& test_case: cases) {
    REQUIRE(string::toBool(test_case.first) == test_case.second);
  }
}

TEST_CASE("test string::testHexEncode", "[test hex encode]") {
  REQUIRE("" == string::to_hex(""));
  REQUIRE("6f" == string::to_hex("o"));
  REQUIRE("666f6f626172" == string::to_hex("foobar"));
  REQUIRE("000102030405060708090a0b0c0d0e0f" == string::to_hex(gsl::make_span(std::vector<uint8_t>{
      0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f}).as_span<const std::byte>()));
  REQUIRE("6F" == string::to_hex("o", true /*uppercase*/));
  REQUIRE("666F6F626172" == string::to_hex("foobar", true /*uppercase*/));
  REQUIRE("000102030405060708090A0B0C0D0E0F" == string::to_hex(gsl::make_span(std::vector<uint8_t>{
      0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f}).as_span<const std::byte>(), true /*uppercase*/));
}

TEST_CASE("test string::testHexDecode", "[test hex decode]") {
  REQUIRE(string::from_hex("").empty());
  REQUIRE("o" == string::from_hex("6f", utils::as_string));
  REQUIRE("o" == string::from_hex("6F", utils::as_string));
  REQUIRE("foobar" == string::from_hex("666f6f626172", utils::as_string));
  REQUIRE("foobar" == string::from_hex("666F6F626172", utils::as_string));
  REQUIRE("foobar" == string::from_hex("66:6F:6F:62:61:72", utils::as_string));
  REQUIRE("foobar" == string::from_hex("66 6F 6F 62 61 72", utils::as_string));
  REQUIRE(std::string({0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f}) == string::from_hex("000102030405060708090a0b0c0d0e0f", utils::as_string));
  REQUIRE(std::string({0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f}) == string::from_hex("000102030405060708090A0B0C0D0E0F", utils::as_string));

  REQUIRE_THROWS_WITH(string::from_hex("666f6f62617"), "Hexencoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_hex("666f6f6261 7"), "Hexencoded string is malformed");
}

TEST_CASE("test string::testHexEncodeDecode", "[test hex encode decode]") {
  std::mt19937 gen(std::random_device{}());
  for (size_t i = 0U; i < 1024U; i++) {
    const bool uppercase = gen() % 2;
    const size_t length = gen() % 1024;
    std::vector<std::byte> data(length);
    std::generate_n(data.begin(), data.size(), [&]() -> std::byte {
      return static_cast<std::byte>(gen() % 256);
    });
    auto hex = string::to_hex(data, uppercase);
    REQUIRE(data == string::from_hex(hex));
  }
}

TEST_CASE("test string::testBase64Encode", "[test base64 encode]") {
  REQUIRE("" == string::to_base64(""));

  REQUIRE("bw==" == string::to_base64("o"));
  REQUIRE("b28=" == string::to_base64("oo"));
  REQUIRE("b29v" == string::to_base64("ooo"));
  REQUIRE("b29vbw==" == string::to_base64("oooo"));
  REQUIRE("b29vb28=" == string::to_base64("ooooo"));
  REQUIRE("b29vb29v" == string::to_base64("oooooo"));

  REQUIRE("bw" == string::to_base64("o", false /*url*/, false /*padded*/));
  REQUIRE("b28" == string::to_base64("oo", false /*url*/, false /*padded*/));
  REQUIRE("b29v" == string::to_base64("ooo", false /*url*/, false /*padded*/));
  REQUIRE("b29vbw" == string::to_base64("oooo", false /*url*/, false /*padded*/));
  REQUIRE("b29vb28" == string::to_base64("ooooo", false /*url*/, false /*padded*/));
  REQUIRE("b29vb29v" == string::to_base64("oooooo", false /*url*/, false /*padded*/));

  std::vector<uint8_t> message{
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
      0xbb, 0xf3, 0xdf, 0xbf};

  REQUIRE("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" ==
      string::to_base64(as_bytes(std::span(message))));
  REQUIRE("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" ==
      string::to_base64(as_bytes(std::span(message)), true /*url*/));
}

TEST_CASE("test string::testBase64Decode", "[test base64 decode]") {
  REQUIRE(string::from_base64("", as_string).empty());
  REQUIRE("o" == string::from_base64("bw==", as_string));
  REQUIRE("oo" == string::from_base64("b28=", as_string));
  REQUIRE("ooo" == string::from_base64("b29v", as_string));
  REQUIRE("oooo" == string::from_base64("b29vbw==", as_string));
  REQUIRE("ooooo" == string::from_base64("b29vb28=", as_string));
  REQUIRE("oooooo" == string::from_base64("b29vb29v", as_string));
  REQUIRE("\xfb\xff\xbf" == string::from_base64("-_-_", as_string));
  REQUIRE("\xfb\xff\xbf" == string::from_base64("+/+/", as_string));
  std::string expected = {
      '\x00', '\x10', '\x83', '\x10',
      '\x51', '\x87', '\x20', '\x92',
      '\x8b', '\x30', '\xd3', '\x8f',
      '\x41', '\x14', '\x93', '\x51',
      '\x55', '\x97', '\x61', '\x96',
      '\x9b', '\x71', '\xd7', '\x9f',
      '\x82', '\x18', '\xa3', '\x92',
      '\x59', '\xa7', '\xa2', '\x9a',
      '\xab', '\xb2', '\xdb', '\xaf',
      '\xc3', '\x1c', '\xb3', '\xd3',
      '\x5d', '\xb7', '\xe3', '\x9e',
      '\xbb', '\xf3', '\xdf', '\xbf'};

  REQUIRE(expected == string::from_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", as_string));
  REQUIRE(expected == string::from_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", as_string));

  REQUIRE("foobarbuzz" == string::from_base64("Zm9vYmFyYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == string::from_base64("\r\nZm9vYmFyYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == string::from_base64("Zm9\r\nvYmFyYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == string::from_base64("Zm\r9vYmFy\n\n\n\n\n\n\n\nYnV6eg==", as_string));
  REQUIRE("foobarbuzz" == string::from_base64("\nZ\nm\n9\nv\nY\nm\nF\ny\nY\nn\nV\n6\ne\ng\n=\n=\n", as_string));

  REQUIRE_THROWS_WITH(string::from_base64("a"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aaaaa"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aa="), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aaaaaa="), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aa==?"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aa==a"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aa==="), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("?"), "Base64 encoded string is malformed");
  REQUIRE_THROWS_WITH(string::from_base64("aaaa?"), "Base64 encoded string is malformed");
}

TEST_CASE("test string::testBase64EncodeDecode", "[test base64 encode decode]") {
  std::mt19937 gen(std::random_device{}());
  for (size_t i = 0U; i < 1024U; i++) {
    const bool url = gen() % 2;
    const bool padded = gen() % 2;
    const size_t length = gen() % 1024;
    std::vector<std::byte> data(length);
    std::generate_n(data.begin(), data.size(), [&]() -> std::byte {
      return static_cast<std::byte>(gen() % 256);
    });
    auto base64 = string::to_base64(data, url, padded);
    REQUIRE(data == string::from_base64(base64));
  }
}

TEST_CASE("test string::testJoinPack", "[test join_pack]") {
  std::string stdstr = "std::string";
  std::string_view strview = "std::string_view";
  const char* cstr = "c string";
  const char carr[] = "char array";  // NOLINT(cppcoreguidelines-avoid-c-arrays): testing const char[] on purpose
  REQUIRE(string::join_pack("rvalue c string, ", cstr, std::string{", rval std::string, "}, stdstr, ", ", strview, ", ", carr)
      == "rvalue c string, c string, rval std::string, std::string, std::string_view, char array");

  // clang can't use the constexpr string implementation of libstdc++
#if defined(__cpp_lib_constexpr_string) && __cpp_lib_constexpr_string >= 201907L && (!defined(__clang__) || defined(_LIBCPP_VERSION))
  STATIC_REQUIRE(string::join_pack(std::string{"a"}, std::string_view{"b"}, "c") == "abc");
#else
  REQUIRE(string::join_pack(std::string{"a"}, std::string_view{"b"}, "c") == "abc");
#endif
}

TEST_CASE("test string::testJoinPackWstring", "[test join_pack wstring]") {
  std::wstring stdstr = L"std::string";
  std::wstring_view strview = L"std::string_view";
  const wchar_t* cstr = L"c string";
  const wchar_t carr[] = L"char array";  // NOLINT(cppcoreguidelines-avoid-c-arrays): testing const wchar_t[] on purpose
  REQUIRE(string::join_pack(L"rvalue c string, ", cstr, std::wstring{L", rval std::string, "}, stdstr, L", ", strview, L", ", carr)
      == L"rvalue c string, c string, rval std::string, std::string, std::string_view, char array");
}

namespace detail {
template<typename... Strs>
concept join_pack_works_with_args = requires(Strs... strs) {
  string::join_pack(strs...);
};
}  // namespace detail

TEST_CASE("test string::join_pack can't combine different char types", "[test join_pack negative][different char types]") {
  STATIC_CHECK(!detail::join_pack_works_with_args<const char*&&, const wchar_t*&, std::string, std::wstring, const char*, const wchar_t[]>);  // NOLINT: testing C array
  STATIC_CHECK(!detail::join_pack_works_with_args<std::string, std::wstring>);
  STATIC_CHECK(!detail::join_pack_works_with_args<std::wstring_view, std::string_view>);
  STATIC_CHECK(!detail::join_pack_works_with_args<const char[], std::string, std::wstring>);  // NOLINT: testing C array
}

TEST_CASE("string::replaceOne works correctly", "[replaceOne]") {
  REQUIRE(string::replaceOne("", "x", "y") == "");
  REQUIRE(string::replaceOne("banana", "a", "_") == "b_nana");
  REQUIRE(string::replaceOne("banana", "b", "_") == "_anana");
  REQUIRE(string::replaceOne("banana", "x", "y") == "banana");
  REQUIRE(string::replaceOne("banana", "an", "") == "bana");
  REQUIRE(string::replaceOne("banana", "an", "AN") == "bANana");
  REQUIRE(string::replaceOne("banana", "an", "***") == "b***ana");
  REQUIRE(string::replaceOne("banana", "banana", "kiwi") == "kiwi");
  REQUIRE(string::replaceOne("banana", "banana", "grapefruit") == "grapefruit");
  REQUIRE(string::replaceOne("fruit", "", "grape") == "grapefruit");
}

TEST_CASE("string::replaceAll works correctly", "[replaceAll]") {
  auto replaceAll = [](std::string input, const std::string& from, const std::string& to) -> std::string {
    return string::replaceAll(input, from, to);
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

TEST_CASE("string::countOccurrences works correctly", "[countOccurrences]") {
  REQUIRE(string::countOccurrences("", "a") == std::make_pair(size_t{0}, size_t{0}));
  REQUIRE(string::countOccurrences("abc", "a") == std::make_pair(size_t{0}, size_t{1}));
  REQUIRE(string::countOccurrences("abc", "b") == std::make_pair(size_t{1}, size_t{1}));
  REQUIRE(string::countOccurrences("abc", "x") == std::make_pair(size_t{0}, size_t{0}));
  REQUIRE(string::countOccurrences("banana", "a") == std::make_pair(size_t{5}, size_t{3}));
  REQUIRE(string::countOccurrences("banana", "an") == std::make_pair(size_t{3}, size_t{2}));
  REQUIRE(string::countOccurrences("aaaaaaaa", "aaa") == std::make_pair(size_t{3}, size_t{2}));  // overlapping occurrences are not counted
  REQUIRE(string::countOccurrences("abc", "") == std::make_pair(size_t{3}, size_t{4}));  // "" occurs at the start, between chars, and at the end
  REQUIRE(string::countOccurrences("", "") == std::make_pair(size_t{0}, size_t{1}));
}

TEST_CASE("string::removeFramingCharacters works correctly", "[removeFramingCharacters]") {
  REQUIRE(string::removeFramingCharacters("", 'a') == "");
  REQUIRE(string::removeFramingCharacters("a", 'a') == "a");
  REQUIRE(string::removeFramingCharacters("aa", 'a') == "");
  REQUIRE(string::removeFramingCharacters("\"abba\"", '"') == "abba");
  REQUIRE(string::removeFramingCharacters("\"\"abba\"\"", '"') == "\"abba\"");
}

// ignore terminating \0 character
template<size_t N>
std::span<const std::byte> from_cstring(const char (& str)[N]) {  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  return as_bytes(std::span<const char>(str, N - 1));
}

TEST_CASE("string::escapeUnprintableBytes", "[escapeUnprintableBytes]") {
  REQUIRE(string::escapeUnprintableBytes(from_cstring("abcd")) == "abcd");
  REQUIRE(string::escapeUnprintableBytes(from_cstring("ab\n\r\t\v\fde")) == "ab\\n\\r\\t\\v\\fde");
  REQUIRE(string::escapeUnprintableBytes(from_cstring("ab\x00""c\x01""d")) == "ab\\x00c\\x01d");
}

TEST_CASE("string::matchesSequence works correctly", "[matchesSequence]") {
  REQUIRE(string::matchesSequence("abcdef", {"abc", "def"}));
  REQUIRE(!string::matchesSequence("abcef", {"abc", "def"}));
  REQUIRE(string::matchesSequence("xxxabcxxxdefxxx", {"abc", "def"}));
  REQUIRE(!string::matchesSequence("defabc", {"abc", "def"}));
  REQUIRE(string::matchesSequence("xxxabcxxxabcxxxdefxxx", {"abc", "def"}));
  REQUIRE(string::matchesSequence("xxxabcxxxabcxxxdefxxx", {"abc", "abc", "def"}));
  REQUIRE(!string::matchesSequence("xxxabcxxxdefxxx", {"abc", "abc", "def"}));
}

TEST_CASE("string::toLower and toUpper tests") {
  CHECK(string::toUpper("Lorem ipsum dolor sit amet") == "LOREM IPSUM DOLOR SIT AMET");
  CHECK(string::toLower("Lorem ipsum dolor sit amet") == "lorem ipsum dolor sit amet");

  CHECK(string::toUpper("SuSpenDISse") == "SUSPENDISSE");
  CHECK(string::toLower("SuSpenDISse") == "suspendisse");
}

TEST_CASE("string::splitToValueAndUnit tests") {
  int64_t value = 0;
  std::string unit_str;
  SECTION("Simple case") {
    CHECK(string::splitToValueAndUnit("1 horse", value, unit_str));
    CHECK(value == 1);
    CHECK(unit_str == "horse");
  }

  SECTION("Without whitespace") {
    CHECK(string::splitToValueAndUnit("112KiB", value, unit_str));
    CHECK(value == 112);
    CHECK(unit_str == "KiB");
  }

  SECTION("Additional whitespace in the middle") {
    CHECK(string::splitToValueAndUnit("100    hOrSe", value, unit_str));
    CHECK(value == 100);
    CHECK(unit_str == "hOrSe");
  }

  SECTION("Invalid value") {
    CHECK_FALSE(string::splitToValueAndUnit("one horse", value, unit_str));
  }

  SECTION("Empty string") {
    CHECK_FALSE(string::splitToValueAndUnit("", value, unit_str));
  }
}

TEST_CASE("string::parseCharacter tests") {
  CHECK(string::parseCharacter("a") == 'a');
  CHECK(string::parseCharacter("\\n") == '\n');
  CHECK(string::parseCharacter("\\t") == '\t');
  CHECK(string::parseCharacter("\\r") == '\r');
  CHECK(string::parseCharacter("\\") == '\\');
  CHECK(string::parseCharacter("\\\\") == '\\');

  CHECK_FALSE(string::parseCharacter("\\s").has_value());
  CHECK_FALSE(string::parseCharacter("\\?").has_value());
  CHECK_FALSE(string::parseCharacter("abc").has_value());
  CHECK_FALSE(string::parseCharacter("\\nd").has_value());
  CHECK(string::parseCharacter("") == std::nullopt);
}

TEST_CASE("string::replaceEscapedCharacters tests") {
  CHECK(string::replaceEscapedCharacters("a") == "a");
  CHECK(string::replaceEscapedCharacters(R"(\n)") == "\n");
  CHECK(string::replaceEscapedCharacters(R"(\t)") == "\t");
  CHECK(string::replaceEscapedCharacters(R"(\r)") == "\r");
  CHECK(string::replaceEscapedCharacters(R"(\s)") == "\\s");
  CHECK(string::replaceEscapedCharacters(R"(\\ foo \)") == "\\ foo \\");
  CHECK(string::replaceEscapedCharacters(R"(\\s)") == "\\s");
  CHECK(string::replaceEscapedCharacters(R"(\r\n)") == "\r\n");
  CHECK(string::replaceEscapedCharacters(R"(foo\nbar)") == "foo\nbar");
  CHECK(string::replaceEscapedCharacters(R"(\\n)") == "\\n");
}

TEST_CASE("string::snakeCaseToPascalCase tests") {
  CHECK(string::snakeCaseToPascalCase("NotAThing") == "Notathing");
  CHECK(string::snakeCaseToPascalCase("one_thing_other") == "OneThingOther");
  CHECK(string::snakeCaseToPascalCase("trailing_underSCORE_") == "TrailingUnderscore");
  CHECK(string::snakeCaseToPascalCase("HTTP_GET_count") == "HttpGetCount");
  CHECK(string::snakeCaseToPascalCase("_leading_underscore") == "LeadingUnderscore");
  CHECK(string::snakeCaseToPascalCase("multiple__underscores") == "MultipleUnderscores");
  CHECK(string::snakeCaseToPascalCase("some_123_\n\rchars") == "Some123\n\rchars");
}

#ifdef WIN32
TEST_CASE("Conversion from UTF-8 strings to UTF-16 strings works") {
  using org::apache::nifi::minifi::utils::to_wstring;

  CHECK(to_wstring("árvíztűrő tükörfúrógép") == L"árvíztűrő tükörfúrógép");
  CHECK(to_wstring("Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.") == L"Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.");
  CHECK(to_wstring("가나다라마바사아자차카타파하") == L"가나다라마바사아자차카타파하");
  CHECK(to_wstring("العربية تجربة") == L"العربية تجربة");
  CHECK(to_wstring("פטכןצימסעואבגדהוזחטייכלמנסעפצקרשת") == L"פטכןצימסעואבגדהוזחטייכלמנסעפצקרשת");
}

TEST_CASE("Conversion from UTF-16 strings to UTF-8 strings works") {
  using org::apache::nifi::minifi::utils::to_string;

  CHECK(to_string(L"árvíztűrő tükörfúrógép") == "árvíztűrő tükörfúrógép");
  CHECK(to_string(L"Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.") == "Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.");
  CHECK(to_string(L"가나다라마바사아자차카타파하") == "가나다라마바사아자차카타파하");
  CHECK(to_string(L"العربية تجربة") == "العربية تجربة");
  CHECK(to_string(L"פטכןצימסעואבגדהוזחטייכלמנסעפצקרשת") == "פטכןצימסעואבגדהוזחטייכלמנסעפצקרשת");
}
#endif

}  // namespace org::apache::nifi::minifi::utils

// NOLINTEND(readability-container-size-empty)
