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

#include "core/Property.h"
#include "../../include/core/Property.h"
#include "utils/StringUtils.h"
#include "../TestBase.h"
namespace {
enum class ParsingStatus { ParsingFail , ParsingSuccessful , ValuesMatch };
enum class ConversionTestTarget { MS, NS };

ParsingStatus checkTimeValue(const std::string &input, int64_t t1, core::TimeUnit t2) {
  int64_t TimeVal = 0;
  core::TimeUnit unit;
  bool parsing_succeeded = org::apache::nifi::minifi::core::Property::StringToTime(input, TimeVal, unit);
  if (parsing_succeeded) {
    if (TimeVal == t1 && unit == t2) {
      return ParsingStatus::ValuesMatch;
    } else {
      return ParsingStatus::ParsingSuccessful;
    }
  } else {
    return ParsingStatus::ParsingFail;
  }
}

bool conversionTest(uint64_t number, core::TimeUnit unit, uint64_t check, ConversionTestTarget conversionUnit) {
  uint64_t out = 0;
  bool returnStatus = false;
  if (conversionUnit == ConversionTestTarget::NS) {
    returnStatus = org::apache::nifi::minifi::core::Property::ConvertTimeUnitToNS(number, unit, out);
  } else if (conversionUnit == ConversionTestTarget::MS) {
    returnStatus = org::apache::nifi::minifi::core::Property::ConvertTimeUnitToMS(number, unit, out);
  }
  return returnStatus && out == check;
}

TEST_CASE("Test Time Conversion", "[testConversion]") {
  uint64_t out;
  REQUIRE(true == conversionTest(2000000, core::TimeUnit::NANOSECOND, 2, ConversionTestTarget::MS));
  REQUIRE(true == conversionTest(5000, core::TimeUnit::MICROSECOND, 5, ConversionTestTarget::MS));
  REQUIRE(true == conversionTest(3, core::TimeUnit::MILLISECOND, 3, ConversionTestTarget::MS));
  REQUIRE(true == conversionTest(5, core::TimeUnit::SECOND, 5000, ConversionTestTarget::MS));
  REQUIRE(true == conversionTest(2, core::TimeUnit::MINUTE, 120000, ConversionTestTarget::MS));
  REQUIRE(true == conversionTest(1, core::TimeUnit::HOUR, 3600000, ConversionTestTarget::MS));
  REQUIRE(true == conversionTest(1, core::TimeUnit::DAY, 86400000, ConversionTestTarget::MS));

  REQUIRE(true == conversionTest(5000, core::TimeUnit::NANOSECOND, 5000, ConversionTestTarget::NS));
  REQUIRE(true == conversionTest(2, core::TimeUnit::MICROSECOND, 2000, ConversionTestTarget::NS));
  REQUIRE(true == conversionTest(3, core::TimeUnit::MILLISECOND, 3000000, ConversionTestTarget::NS));
  REQUIRE(true == conversionTest(7, core::TimeUnit::SECOND, 7000000000, ConversionTestTarget::NS));
  REQUIRE(true == conversionTest(1, core::TimeUnit::MINUTE, 60000000000, ConversionTestTarget::NS));
  REQUIRE(true == conversionTest(1, core::TimeUnit::HOUR, 3600000000000, ConversionTestTarget::NS));
  REQUIRE(true == conversionTest(1, core::TimeUnit::DAY, 86400000000000, ConversionTestTarget::NS));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::ConvertTimeUnitToNS(23, static_cast<core::TimeUnit>(-1), out));
  REQUIRE(false == org::apache::nifi::minifi::core::Property::ConvertTimeUnitToMS(23, static_cast<core::TimeUnit>(-1), out));
}

TEST_CASE("Test Is it Time", "[testTime]") {
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("1 SEC", 1, core::TimeUnit::SECOND));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("1d", 1, core::TimeUnit::DAY));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("10 days", 10, core::TimeUnit::DAY));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("100ms", 100, core::TimeUnit::MILLISECOND));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("20 us", 20, core::TimeUnit::MICROSECOND));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("1ns", 1, core::TimeUnit::NANOSECOND));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("1min", 1, core::TimeUnit::MINUTE));
  REQUIRE(ParsingStatus::ValuesMatch == checkTimeValue("1 hour", 1, core::TimeUnit::HOUR));

  REQUIRE(ParsingStatus::ParsingSuccessful == checkTimeValue("100 SEC", 100, core::TimeUnit::MICROSECOND));
  REQUIRE(ParsingStatus::ParsingSuccessful == checkTimeValue("10 ms", 1, core::TimeUnit::HOUR));
  REQUIRE(ParsingStatus::ParsingSuccessful == checkTimeValue("100us", 100, core::TimeUnit::HOUR));
  REQUIRE(ParsingStatus::ParsingSuccessful == checkTimeValue("100 ns", 100, core::TimeUnit::MILLISECOND));
  REQUIRE(ParsingStatus::ParsingSuccessful == checkTimeValue("1 minute", 10, core::TimeUnit::MINUTE));

  REQUIRE(ParsingStatus::ParsingFail == checkTimeValue("5 apples", 1, core::TimeUnit::HOUR));
  REQUIRE(ParsingStatus::ParsingFail == checkTimeValue("1 year", 1, core::TimeUnit::DAY));
}

TEST_CASE("Test Trimmer Right", "[testTrims]") {
  std::string test = "a quick brown fox jumped over the road\t\n";

  REQUIRE(test.c_str()[test.length() - 1] == '\n');
  REQUIRE(test.c_str()[test.length() - 2] == '\t');
  test = org::apache::nifi::minifi::utils::StringUtils::trimRight(test);

  REQUIRE(test.c_str()[test.length() - 1] == 'd');
  REQUIRE(test.c_str()[test.length() - 2] == 'a');

  test = "a quick brown fox jumped over the road\v\t";

  REQUIRE(test.c_str()[test.length() - 1] == '\t');
  REQUIRE(test.c_str()[test.length() - 2] == '\v');

  test = org::apache::nifi::minifi::utils::StringUtils::trimRight(test);

  REQUIRE(test.c_str()[test.length() - 1] == 'd');
  REQUIRE(test.c_str()[test.length() - 2] == 'a');

  test = "a quick brown fox jumped over the road \f";

  REQUIRE(test.c_str()[test.length() - 1] == '\f');
  REQUIRE(test.c_str()[test.length() - 2] == ' ');

  test = org::apache::nifi::minifi::utils::StringUtils::trimRight(test);

  REQUIRE(test.c_str()[test.length() - 1] == 'd');
}

TEST_CASE("Test Trimmer Left", "[testTrims]") {
  std::string test = "\t\na quick brown fox jumped over the road\t\n";

  REQUIRE(test.c_str()[0] == '\t');
  REQUIRE(test.c_str()[1] == '\n');

  test = org::apache::nifi::minifi::utils::StringUtils::trimLeft(test);

  REQUIRE(test.c_str()[0] == 'a');
  REQUIRE(test.c_str()[1] == ' ');

  test = "\v\ta quick brown fox jumped over the road\v\t";

  REQUIRE(test.c_str()[0] == '\v');
  REQUIRE(test.c_str()[1] == '\t');

  test = org::apache::nifi::minifi::utils::StringUtils::trimLeft(test);

  REQUIRE(test.c_str()[0] == 'a');
  REQUIRE(test.c_str()[1] == ' ');

  test = " \fa quick brown fox jumped over the road \f";

  REQUIRE(test.c_str()[0] == ' ');
  REQUIRE(test.c_str()[1] == '\f');

  test = org::apache::nifi::minifi::utils::StringUtils::trimLeft(test);

  REQUIRE(test.c_str()[0] == 'a');
  REQUIRE(test.c_str()[1] == ' ');
}

TEST_CASE("Test Permissions Conversion", "[testPermissions]") {
  uint32_t permissions = 0U;

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("0777", permissions));
  REQUIRE(0777 == permissions);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("0000", permissions));
  REQUIRE(0000 == permissions);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("0644", permissions));
  REQUIRE(0644 == permissions);

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("0999", permissions));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("999", permissions));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("0644a", permissions));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("07777", permissions));

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("rwxrwxrwx", permissions));
  REQUIRE(0777 == permissions);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("---------", permissions));
  REQUIRE(0000 == permissions);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("rwxrw-r--", permissions));
  REQUIRE(0764 == permissions);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToPermissions("r--r--r--", permissions));
  REQUIRE(0444 == permissions);

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("wxrwxrwxr", permissions));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("foobarfoo", permissions));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToPermissions("foobar", permissions));
}

TEST_CASE("Test DateTime Conversion", "[testDateTime]") {
  int64_t timestamp = 0LL;

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToDateTime("1970-01-01T00:00:00Z", timestamp));
  REQUIRE(0LL == timestamp);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToDateTime("1970-01-01T00:59:59Z", timestamp));
  REQUIRE(3600 - 1 == timestamp);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToDateTime("2000-06-17T12:34:21Z", timestamp));
  REQUIRE(961245261LL == timestamp);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToDateTime("2038-01-19T03:14:07Z", timestamp));
  REQUIRE(2147483647LL == timestamp);

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToDateTime("2065-01-24T05:20:00Z", timestamp));
  REQUIRE(3000000000LL == timestamp);

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToDateTime("1970-01-01A00:00:00Z", timestamp));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToDateTime("1970-01-01T00:00:00", timestamp));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToDateTime("1970-01-01T00:00:00Zfoo", timestamp));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToDateTime("1969-01-01T00:00:00Z", timestamp));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToDateTime("1970-13-01T00:00:00Z", timestamp));

  REQUIRE(false == org::apache::nifi::minifi::core::Property::StringToDateTime("foobar", timestamp));
}
}   // namespace
