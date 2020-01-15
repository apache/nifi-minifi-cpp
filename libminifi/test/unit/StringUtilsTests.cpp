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
#include "utils/Environment.h"

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


  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist", "computer", 0);

  std::string expected = "hello world computer";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv2", "[test split classname]") {
  std::string test_string = "hello world ${blahblahnamenamenotexist";

  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist", "computer");

  std::string expected = "hello world ${blahblahnamenamenotexist";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv3", "[test split classname]") {
  std::string test_string = "hello world $${blahblahnamenamenotexist}";

  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist", "computer");

  std::string expected = "hello world $computer";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv4", "[test split classname]") {
  std::string test_string = "hello world \\${blahblahnamenamenotexist}";

  utils::Environment::setEnvironmentVariable("blahblahnamenamenotexist", "computer");

  std::string expected = "hello world ${blahblahnamenamenotexist}";

  REQUIRE(expected == StringUtils::replaceEnvironmentVariables(test_string));
}

TEST_CASE("TestStringUtils::testEnv5", "[test split classname]") {
  // can't use blahblahnamenamenotexist because the utils::Environment::setEnvironmentVariable in other functions may have already set it
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

TEST_CASE("TestStringUtils::trim", "[test trim]") {
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

TEST_CASE("TestStringUtils::testHexEncode", "[test hex encode]") {
  REQUIRE("" == StringUtils::to_hex(""));
  REQUIRE("6f" == StringUtils::to_hex("o"));
  REQUIRE("666f6f626172" == StringUtils::to_hex("foobar"));
  REQUIRE("000102030405060708090a0b0c0d0e0f" == StringUtils::to_hex(std::vector<uint8_t>{0x00, 0x01, 0x02, 0x03,
                                                                                         0x04, 0x05, 0x06, 0x07,
                                                                                         0x08, 0x09, 0x0a, 0x0b,
                                                                                         0x0c, 0x0d, 0x0e, 0x0f}));
  REQUIRE("6F" == StringUtils::to_hex("o", true /*uppercase*/));
  REQUIRE("666F6F626172" == StringUtils::to_hex("foobar", true /*uppercase*/));
  REQUIRE("000102030405060708090A0B0C0D0E0F" == StringUtils::to_hex(std::vector<uint8_t>{0x00, 0x01, 0x02, 0x03,
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

  REQUIRE_THROWS_WITH(StringUtils::from_hex("666f6f62617"), "Hexencoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_hex("666f6f6261 7"), "Hexencoded string is malformatted");
}

TEST_CASE("TestStringUtils::testHexEncodeDecode", "[test hex encode decode]") {
  std::mt19937 gen(std::random_device { }());
  for (size_t i = 0U; i < 1024U; i++) {
    const bool uppercase = gen() % 2;
    const size_t length = gen() % 1024;
    std::vector<uint8_t> data(length);
    std::generate_n(data.begin(), data.size(), [&]() -> uint8_t {
      return gen() % 256;
    });
    auto hex = utils::StringUtils::to_hex(data.data(), data.size(), uppercase);
    REQUIRE(data == utils::StringUtils::from_hex(hex.data(), hex.size()));
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
    StringUtils::to_base64(std::vector<uint8_t>{0x00, 0x10, 0x83, 0x10,
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
                                                0xbb, 0xf3, 0xdf, 0xbf}));
  REQUIRE("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" ==
    StringUtils::to_base64(std::vector<uint8_t>{0x00, 0x10, 0x83, 0x10,
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
                                                0xbb, 0xf3, 0xdf, 0xbf}, true /*url*/));
}

TEST_CASE("TestStringUtils::testBase64Decode", "[test base64 decode]") {
  REQUIRE("" == StringUtils::from_base64(""));
  REQUIRE("o" == StringUtils::from_base64("bw=="));
  REQUIRE("oo" == StringUtils::from_base64("b28="));
  REQUIRE("ooo" == StringUtils::from_base64("b29v"));
  REQUIRE("oooo" == StringUtils::from_base64("b29vbw=="));
  REQUIRE("ooooo" == StringUtils::from_base64("b29vb28="));
  REQUIRE("oooooo" == StringUtils::from_base64("b29vb29v"));
  REQUIRE("\xfb\xff\xbf" == StringUtils::from_base64("-_-_"));
  REQUIRE("\xfb\xff\xbf" == StringUtils::from_base64("+/+/"));
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
                        -69,  -13,  -33,  -65}) == StringUtils::from_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"));
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
                        -69,  -13,  -33,  -65}) == StringUtils::from_base64("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"));

  REQUIRE("foobarbuzz" == StringUtils::from_base64("Zm9vYmFyYnV6eg=="));
  REQUIRE("foobarbuzz"== StringUtils::from_base64("\r\nZm9vYmFyYnV6eg=="));
  REQUIRE("foobarbuzz" == StringUtils::from_base64("Zm9\r\nvYmFyYnV6eg=="));
  REQUIRE("foobarbuzz" == StringUtils::from_base64("Zm\r9vYmFy\n\n\n\n\n\n\n\nYnV6eg=="));
  REQUIRE("foobarbuzz" == StringUtils::from_base64("\nZ\nm\n9\nv\nY\nm\nF\ny\nY\nn\nV\n6\ne\ng\n=\n=\n"));

  REQUIRE_THROWS_WITH(StringUtils::from_base64("a"), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aaaaa"), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa="), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aaaaaa="), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa==?"), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa==a"), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aa==="), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("?"), "Base64 encoded string is malformatted");
  REQUIRE_THROWS_WITH(StringUtils::from_base64("aaaa?"), "Base64 encoded string is malformatted");
}

TEST_CASE("TestStringUtils::testBase64EncodeDecode", "[test base64 encode decode]") {
  std::mt19937 gen(std::random_device { }());
  for (size_t i = 0U; i < 1024U; i++) {
    const bool url = gen() % 2;
    const bool padded = gen() % 2;
    const size_t length = gen() % 1024;
    std::vector<uint8_t> data(length);
    std::generate_n(data.begin(), data.size(), [&]() -> uint8_t {
      return gen() % 256;
    });
    auto base64 = utils::StringUtils::to_base64(data.data(), data.size(), url, padded);
    REQUIRE(data == utils::StringUtils::from_base64(base64.data(), base64.size()));
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
