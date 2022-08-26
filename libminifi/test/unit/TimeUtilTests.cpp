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

#include "utils/TimeUtil.h"
#include "../TestBase.h"
#include "../Catch.h"

using namespace std::literals::chrono_literals;

TEST_CASE("parseDateTimeStr() works correctly", "[parseDateTimeStr]") {
  using org::apache::nifi::minifi::utils::timeutils::parseDateTimeStr;

  CHECK(*parseDateTimeStr("1970-01-01T00:00:00Z") == std::chrono::sys_seconds{0s});
  CHECK(*parseDateTimeStr("1970-01-01T00:59:59Z") == std::chrono::sys_seconds{1h - 1s});

  CHECK(*parseDateTimeStr("1970-01-02T00:00:00Z") == std::chrono::sys_seconds{std::chrono::days(1)});
  CHECK(*parseDateTimeStr("1970-02-01T00:00:00Z") == std::chrono::sys_seconds{31 * std::chrono::days(1)});
  CHECK(*parseDateTimeStr("1971-01-01T00:00:00Z") == std::chrono::sys_seconds{365 * std::chrono::days(1)});

  CHECK(*parseDateTimeStr("1995-02-28T00:00:00Z") == std::chrono::sys_seconds{793929600s});
  CHECK(*parseDateTimeStr("1995-03-01T00:00:00Z") == std::chrono::sys_seconds{793929600s + std::chrono::days(1)});
  CHECK(*parseDateTimeStr("1996-02-28T00:00:00Z") == std::chrono::sys_seconds{825465600s});
  CHECK(*parseDateTimeStr("1996-02-29T00:00:00Z") == std::chrono::sys_seconds{825465600s + std::chrono::days(1)});
  CHECK(*parseDateTimeStr("2000-02-28T00:00:00Z") == std::chrono::sys_seconds{951696000s});
  CHECK(*parseDateTimeStr("2000-02-29T00:00:00Z") == std::chrono::sys_seconds{951696000s + std::chrono::days(1)});
  CHECK(*parseDateTimeStr("2100-02-28T00:00:00Z") == std::chrono::sys_seconds{4107456000s});
  CHECK(*parseDateTimeStr("2100-03-01T00:00:00Z") == std::chrono::sys_seconds{4107456000s + std::chrono::days(1)});

  CHECK(*parseDateTimeStr("2017-12-12T18:54:16Z") == std::chrono::sys_seconds{1513104856s});
  CHECK(*parseDateTimeStr("2024-01-30T23:01:15Z") == std::chrono::sys_seconds{1706655675s});
  CHECK(*parseDateTimeStr("2087-07-31T01:33:50Z") == std::chrono::sys_seconds{3710453630s});
}

TEST_CASE("getDateTimeStr() works correctly", "[getDateTimeStr]") {
  using org::apache::nifi::minifi::utils::timeutils::getDateTimeStr;

  CHECK("1970-01-01T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{0s}));
  CHECK("1970-01-01T00:59:59Z" == getDateTimeStr(std::chrono::sys_seconds{1h - 1s}));

  CHECK("1970-01-02T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{std::chrono::days(1)}));
  CHECK("1970-02-01T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{31 * std::chrono::days(1)}));
  CHECK("1971-01-01T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{365 * std::chrono::days(1)}));

  CHECK("1995-02-28T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{793929600s}));
  CHECK("1995-03-01T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{793929600s + std::chrono::days(1)}));
  CHECK("1996-02-28T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{825465600s}));
  CHECK("1996-02-29T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{825465600s + std::chrono::days(1)}));
  CHECK("2000-02-28T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{951696000s}));
  CHECK("2000-02-29T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{951696000s + std::chrono::days(1)}));
  CHECK("2100-02-28T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{4107456000s}));
  CHECK("2100-03-01T00:00:00Z" == getDateTimeStr(std::chrono::sys_seconds{4107456000s + std::chrono::days(1)}));

  CHECK("2017-12-12T18:54:16Z" == getDateTimeStr(std::chrono::sys_seconds{1513104856s}));
  CHECK("2024-01-30T23:01:15Z" == getDateTimeStr(std::chrono::sys_seconds{1706655675s}));
  CHECK("2087-07-31T01:33:50Z" == getDateTimeStr(std::chrono::sys_seconds{3710453630s}));
}

TEST_CASE("getRFC2616Format() works correctly", "[getRFC2616Format]") {
  using namespace date::literals;
  using namespace std::literals::chrono_literals;
  using date::year_month_day;
  using date::sys_days;
  using org::apache::nifi::minifi::utils::timeutils::getRFC2616Format;

  CHECK("Thu, 01 Jan 1970 00:00:00 UTC" == getRFC2616Format(std::chrono::sys_seconds{0s}));
  CHECK("Thu, 01 Jan 1970 00:59:59 UTC" == getRFC2616Format(std::chrono::sys_seconds{1h - 1s}));

  // Example from https://www.rfc-editor.org/rfc/rfc7231#page-67
  CHECK("Tue, 15 Nov 1994 08:12:31 UTC" == getRFC2616Format(sys_days(year_month_day(1994_y/11/15)) + 8h + 12min + 31s));
}

TEST_CASE("Test time conversion", "[testtimeconversion]") {
  using org::apache::nifi::minifi::utils::timeutils::getTimeStr;
  CHECK("2017-02-16 20:14:56.196" == getTimeStr(std::chrono::system_clock::time_point{1487276096196ms}));
}

TEST_CASE("Test DateTime Conversion", "[testDateTime]") {
  using namespace date::literals;
  using namespace std::literals::chrono_literals;
  using date::year_month_day;
  using date::sys_days;
  using utils::timeutils::parseDateTimeStr;

  CHECK(sys_days(date::year_month_day(1970_y/01/01)) == parseDateTimeStr("1970-01-01T00:00:00Z"));
  CHECK(sys_days(year_month_day(1970_y/01/01)) + 0h + 59min + 59s == parseDateTimeStr("1970-01-01T00:59:59Z"));
  CHECK(sys_days(year_month_day(2000_y/06/17)) + 12h + 34min + 21s == parseDateTimeStr("2000-06-17T12:34:21Z"));
  CHECK(sys_days(year_month_day(2038_y/01/19)) + 3h + 14min + 7s == parseDateTimeStr("2038-01-19T03:14:07Z"));
  CHECK(sys_days(year_month_day(2065_y/01/24)) + 5h + 20min + 0s == parseDateTimeStr("2065-01-24T05:20:00Z"));
  CHECK(sys_days(year_month_day(1969_y/01/01)) == parseDateTimeStr("1969-01-01T00:00:00Z"));

  CHECK_FALSE(utils::timeutils::parseDateTimeStr("1970-01-01A00:00:00Z"));
  CHECK_FALSE(utils::timeutils::parseDateTimeStr("1970-01-01T00:00:00"));
  CHECK_FALSE(utils::timeutils::parseDateTimeStr("1970-01-01T00:00:00Zfoo"));
  CHECK_FALSE(utils::timeutils::parseDateTimeStr("1970-13-01T00:00:00Z"));
  CHECK_FALSE(utils::timeutils::parseDateTimeStr("foobar"));
}

TEST_CASE("Test system_clock epoch", "[systemclockepoch]") {
  using namespace std::chrono;
  time_point<system_clock> epoch;
  time_point<system_clock> unix_epoch_plus_3e9_sec = date::sys_days(date::January / 24 / 2065) + 5h + 20min;
  REQUIRE(epoch.time_since_epoch() == 0s);
  REQUIRE(unix_epoch_plus_3e9_sec.time_since_epoch() == 3000000000s);
}

TEST_CASE("Test clock resolutions", "[clockresolutiontests]") {
  using namespace std::chrono;
  CHECK(std::is_constructible<system_clock::duration, std::chrono::microseconds>::value);  // The resolution of the system_clock is at least microseconds
  CHECK(std::is_constructible<steady_clock::duration, std::chrono::microseconds>::value);  // The resolution of the steady_clock is at least microseconds
  CHECK(std::is_constructible<high_resolution_clock::duration, std::chrono::nanoseconds>::value);  // The resolution of the high_resolution_clock is at least nanoseconds
}

TEST_CASE("Test string to duration conversion", "[timedurationtests]") {
  using org::apache::nifi::minifi::utils::timeutils::StringToDuration;
  auto one_hour = StringToDuration<std::chrono::milliseconds>("1h");
  REQUIRE(one_hour);
  CHECK(one_hour.value() == 1h);
  CHECK(one_hour.value() == 3600s);

  REQUIRE(StringToDuration<std::chrono::milliseconds>("1 hour"));
  REQUIRE(StringToDuration<std::chrono::seconds>("102             hours") == 102h);
  REQUIRE(StringToDuration<std::chrono::days>("102             hours") == std::chrono::days(4));
  REQUIRE(StringToDuration<std::chrono::milliseconds>("5 ns") == 0ms);

  REQUIRE(StringToDuration<std::chrono::seconds>("1d") == std::chrono::days(1));
  REQUIRE(StringToDuration<std::chrono::seconds>("10 days") == std::chrono::days(10));
  REQUIRE(StringToDuration<std::chrono::seconds>("100ms") == 0ms);
  REQUIRE(StringToDuration<std::chrono::seconds>("20 us") == 0s);
  REQUIRE(StringToDuration<std::chrono::seconds>("1ns") == 0ns);
  REQUIRE(StringToDuration<std::chrono::seconds>("1min") == 1min);
  REQUIRE(StringToDuration<std::chrono::seconds>("1 hour") == 1h);
  REQUIRE(StringToDuration<std::chrono::seconds>("100 SEC") == 100s);
  REQUIRE(StringToDuration<std::chrono::seconds>("10 ms") == 0ms);
  REQUIRE(StringToDuration<std::chrono::seconds>("100 ns") == 0ns);
  REQUIRE(StringToDuration<std::chrono::seconds>("1 minute") == 1min);

  REQUIRE(StringToDuration<std::chrono::nanoseconds>("1d") == std::chrono::days(1));
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("10 days") == std::chrono::days(10));
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("100ms") == 100ms);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("20 us") == 20us);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("1ns") == 1ns);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("1min") == 1min);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("1 hour") == 1h);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("100 SEC") == 100s);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("10 ms") == 10ms);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("100 ns") == 100ns);
  REQUIRE(StringToDuration<std::chrono::nanoseconds>("1 minute") == 1min);

  REQUIRE_FALSE(StringToDuration<std::chrono::seconds>("5 apples") == 1s);
  REQUIRE_FALSE(StringToDuration<std::chrono::seconds>("1 year") == 1s);
}

namespace {
date::local_time<std::chrono::seconds> parseLocalTimePoint(const std::string& str) {
  date::local_time<std::chrono::seconds> tp;
  std::stringstream stream(str);
  date::from_stream(stream, "%Y-%m-%d %T", tp);
  return tp;
}
}  // namespace

TEST_CASE("Test roundToNextYear", "[roundingTests]") {
  using org::apache::nifi::minifi::utils::timeutils::roundToNextYear;

  CHECK(parseLocalTimePoint("2022-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("2021-12-21 08:20:53")));
  CHECK(parseLocalTimePoint("2023-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("2022-01-01 13:59:59")));
  CHECK(parseLocalTimePoint("1974-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("1973-02-01 00:00:01")));
  CHECK(parseLocalTimePoint("1971-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("1970-11-03 23:59:59")));
  CHECK(parseLocalTimePoint("2023-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("2022-12-03 15:22:22")));
  CHECK(parseLocalTimePoint("2254-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("2253-05-01 23:59:59")));
  CHECK(parseLocalTimePoint("1951-01-01 00:00:00") == roundToNextYear(parseLocalTimePoint("1950-11-03 23:59:59")));
}

TEST_CASE("Test roundToNextMonth", "[roundingTests]") {
  using org::apache::nifi::minifi::utils::timeutils::roundToNextMonth;

  CHECK(parseLocalTimePoint("2022-01-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2021-12-21 01:00:00")));
  CHECK(parseLocalTimePoint("2022-02-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-01-31 02:00:00")));
  CHECK(parseLocalTimePoint("2022-03-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-02-01 12:00:00")));
  CHECK(parseLocalTimePoint("2022-12-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-11-30 00:00:00")));
  CHECK(parseLocalTimePoint("2023-01-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-12-31 00:00:00")));
  CHECK(parseLocalTimePoint("2022-06-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-05-12 23:00:58")));
  CHECK(parseLocalTimePoint("2022-07-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-06-21 11:00:00")));
  CHECK(parseLocalTimePoint("2022-08-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-07-21 12:12:00")));
  CHECK(parseLocalTimePoint("2022-09-01 00:00:00") == roundToNextMonth(parseLocalTimePoint("2022-08-31 06:00:00")));
}

TEST_CASE("Test roundToNextDay", "[roundingTests]") {
  using org::apache::nifi::minifi::utils::timeutils::roundToNextDay;

  CHECK(parseLocalTimePoint("2021-02-01 00:00:00") == roundToNextDay(parseLocalTimePoint("2021-01-31 01:00:00")));
  CHECK(parseLocalTimePoint("2022-03-01 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-02-28 02:00:00")));
  CHECK(parseLocalTimePoint("2024-02-29 00:00:00") == roundToNextDay(parseLocalTimePoint("2024-02-28 02:00:00")));
  CHECK(parseLocalTimePoint("2023-01-01 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-12-31 12:00:00")));
  CHECK(parseLocalTimePoint("2022-07-11 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-07-10 00:00:00")));
  CHECK(parseLocalTimePoint("2023-01-01 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-12-31 00:00:00")));
  CHECK(parseLocalTimePoint("2022-05-13 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-05-12 23:00:58")));
  CHECK(parseLocalTimePoint("2022-06-22 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-06-21 11:00:00")));
  CHECK(parseLocalTimePoint("2022-07-22 00:00:00") == roundToNextDay(parseLocalTimePoint("2022-07-21 12:12:00")));
}

TEST_CASE("Test roundToNextHour", "[roundingTests]") {
  using org::apache::nifi::minifi::utils::timeutils::roundToNextHour;

  CHECK(parseLocalTimePoint("2021-01-31 02:00:00") == roundToNextHour(parseLocalTimePoint("2021-01-31 01:00:00")));
  CHECK(parseLocalTimePoint("2022-03-01 00:00:00") == roundToNextHour(parseLocalTimePoint("2022-02-28 23:00:00")));
  CHECK(parseLocalTimePoint("2024-02-29 00:00:00") == roundToNextHour(parseLocalTimePoint("2024-02-28 23:00:00")));
  CHECK(parseLocalTimePoint("2022-12-31 13:00:00") == roundToNextHour(parseLocalTimePoint("2022-12-31 12:00:00")));
  CHECK(parseLocalTimePoint("2022-07-10 01:00:00") == roundToNextHour(parseLocalTimePoint("2022-07-10 00:00:00")));
  CHECK(parseLocalTimePoint("2022-12-31 01:00:00") == roundToNextHour(parseLocalTimePoint("2022-12-31 00:00:00")));
  CHECK(parseLocalTimePoint("2022-05-13 00:00:00") == roundToNextHour(parseLocalTimePoint("2022-05-12 23:00:58")));
  CHECK(parseLocalTimePoint("2022-06-21 12:00:00") == roundToNextHour(parseLocalTimePoint("2022-06-21 11:00:00")));
  CHECK(parseLocalTimePoint("2022-07-21 13:00:00") == roundToNextHour(parseLocalTimePoint("2022-07-21 12:12:00")));
}

TEST_CASE("Test roundToNextMinute", "[roundingTests]") {
  using org::apache::nifi::minifi::utils::timeutils::roundToNextMinute;

  CHECK(parseLocalTimePoint("2021-01-31 01:24:00") == roundToNextMinute(parseLocalTimePoint("2021-01-31 01:23:00")));
  CHECK(parseLocalTimePoint("2022-03-01 00:00:00") == roundToNextMinute(parseLocalTimePoint("2022-02-28 23:59:59")));
  CHECK(parseLocalTimePoint("2024-02-28 23:01:00") == roundToNextMinute(parseLocalTimePoint("2024-02-28 23:00:00")));
  CHECK(parseLocalTimePoint("2022-12-31 12:01:00") == roundToNextMinute(parseLocalTimePoint("2022-12-31 12:00:00")));
  CHECK(parseLocalTimePoint("2022-07-10 00:01:00") == roundToNextMinute(parseLocalTimePoint("2022-07-10 00:00:00")));
  CHECK(parseLocalTimePoint("2022-12-31 00:01:00") == roundToNextMinute(parseLocalTimePoint("2022-12-31 00:00:00")));
  CHECK(parseLocalTimePoint("2022-05-12 23:01:00") == roundToNextMinute(parseLocalTimePoint("2022-05-12 23:00:58")));
  CHECK(parseLocalTimePoint("2022-06-21 11:01:00") == roundToNextMinute(parseLocalTimePoint("2022-06-21 11:00:00")));
  CHECK(parseLocalTimePoint("2022-07-21 12:13:00") == roundToNextMinute(parseLocalTimePoint("2022-07-21 12:12:00")));
}

TEST_CASE("Test roundToNextSecond", "[roundingTests]") {
  using org::apache::nifi::minifi::utils::timeutils::roundToNextSecond;

  CHECK(parseLocalTimePoint("2021-01-31 01:23:01") == roundToNextSecond(parseLocalTimePoint("2021-01-31 01:23:00")));
  CHECK(parseLocalTimePoint("2022-03-01 00:00:00") == roundToNextSecond(parseLocalTimePoint("2022-02-28 23:59:59")));
  CHECK(parseLocalTimePoint("2024-02-28 23:00:01") == roundToNextSecond(parseLocalTimePoint("2024-02-28 23:00:00")));
  CHECK(parseLocalTimePoint("2022-12-31 12:00:01") == roundToNextSecond(parseLocalTimePoint("2022-12-31 12:00:00")));
  CHECK(parseLocalTimePoint("2022-07-10 00:00:01") == roundToNextSecond(parseLocalTimePoint("2022-07-10 00:00:00")));
  CHECK(parseLocalTimePoint("2022-12-31 00:00:01") == roundToNextSecond(parseLocalTimePoint("2022-12-31 00:00:00")));
  CHECK(parseLocalTimePoint("2022-05-12 23:00:59") == roundToNextSecond(parseLocalTimePoint("2022-05-12 23:00:58")));
  CHECK(parseLocalTimePoint("2022-06-21 11:00:01") == roundToNextSecond(parseLocalTimePoint("2022-06-21 11:00:00")));
  CHECK(parseLocalTimePoint("2022-07-21 12:12:01") == roundToNextSecond(parseLocalTimePoint("2022-07-21 12:12:00")));
}
