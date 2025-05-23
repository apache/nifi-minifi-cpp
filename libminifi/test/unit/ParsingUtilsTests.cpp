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

#include <string>
#include "unit/Catch.h"
#include "utils/ParsingUtils.h"

namespace org::apache::nifi::minifi::parsing::test {
TEST_CASE("Test boolean parsing") {
  CHECK(false == *parseBool("false"));
  CHECK(true == *parseBool("true"));
  CHECK(false == *parseBool("fAlSe"));
  CHECK(true == *parseBool("TRUE"));
  CHECK(core::ParsingErrorCode::GeneralParsingError == parseBool("foo").error());
  CHECK(core::ParsingErrorCode::GeneralParsingError == parseBool("true dat").error());
  CHECK(core::ParsingErrorCode::GeneralParsingError == parseBool("bar false").error());
}

TEST_CASE("Test integral parsing") {
  CHECK(8000U == parseIntegral<uint64_t>("8000"));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError) == parseIntegral<uint64_t>("8000 banana"));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError) == parseIntegral<uint64_t>("-8000"));


  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum) == parseIntegralMinMax<uint64_t>("10", 3, 8));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum) == parseIntegralMinMax<uint64_t>("2", 3, 8));

  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError) == parseIntegral<int16_t>("90000"));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError) == parseIntegral<int16_t>("-90000"));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError) == parseIntegral<int16_t>("8000 banana"));
}

TEST_CASE("Test data size parsing") {
  CHECK(8000U == parseDataSize("8000"));
  CHECK(8192000U == parseDataSize("8000 kB"));

  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::GeneralParsingError) == parseDataSize("8000 banana"));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::LargerThanMaximum) == parseDataSizeMinMax("9 MB", 3000, 8000));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::SmallerThanMinimum) == parseDataSizeMinMax("0 GB", 3000, 8000));
  CHECK(nonstd::make_unexpected(core::ParsingErrorCode::OverflowError) == parseDataSize("20000 PB"));
}

TEST_CASE("Test Permissions Parsing") {
  CHECK(0777U == parseUnixOctalPermissions("0777"));
  CHECK(0000U == parseUnixOctalPermissions("0000"));
  CHECK(0644U == parseUnixOctalPermissions("0644"));

  CHECK_FALSE(parseUnixOctalPermissions("0999"));
  CHECK_FALSE(parseUnixOctalPermissions("999"));
  CHECK_FALSE(parseUnixOctalPermissions("0644a"));
  CHECK_FALSE(parseUnixOctalPermissions("07777"));

  CHECK(0777U == parseUnixOctalPermissions("rwxrwxrwx"));
  CHECK(0000U == parseUnixOctalPermissions("---------"));
  CHECK(0764U == parseUnixOctalPermissions("rwxrw-r--"));
  CHECK(0444U == parseUnixOctalPermissions("r--r--r--"));

  CHECK_FALSE(parseUnixOctalPermissions("wxrwxrwxr"));
  CHECK_FALSE(parseUnixOctalPermissions("foobarfoo"));
  CHECK_FALSE(parseUnixOctalPermissions("foobar"));

  CHECK_FALSE(parseUnixOctalPermissions("0644 banana"));
}

TEST_CASE("Test Duration Parsing") {
  using namespace std::literals::chrono_literals;
  CHECK(12s == parseDuration("12s"));
  CHECK_FALSE(parseDuration("12ss"));
  CHECK_FALSE(parseDuration("2 fortnights"));
}
}  // namespace org::apache::nifi::minifi::parsing::test
