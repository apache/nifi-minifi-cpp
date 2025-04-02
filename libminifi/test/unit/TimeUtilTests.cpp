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
#include "unit/TestBase.h"
#include "unit/Catch.h"

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
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
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
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
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
  std::chrono::time_point<std::chrono::system_clock> epoch;
  std::chrono::time_point<std::chrono::system_clock> unix_epoch_plus_3e9_sec = date::sys_days(date::January / 24 / 2065) + 5h + 20min;
  REQUIRE(epoch.time_since_epoch() == 0s);
  REQUIRE(unix_epoch_plus_3e9_sec.time_since_epoch() == 3000000000s);
}

#ifdef WIN32
TEST_CASE("Test windows file_clock duration period and epoch") {
  static_assert(std::ratio_equal_v<std::chrono::file_clock::duration::period, std::ratio<1, 10000000>>, "file_clock duration tick period must be 100 nanoseconds");
  auto file_clock_epoch = std::chrono::file_clock::time_point{};
  auto file_clock_epoch_as_sys_time = utils::file::to_sys(file_clock_epoch);
  std::chrono::system_clock::time_point expected_windows_file_epoch = date::sys_days(date::January / 1 / 1601);
  CHECK(file_clock_epoch_as_sys_time == expected_windows_file_epoch);
}

TEST_CASE("Test windows FILETIME epoch") {
  SYSTEMTIME system_time;
  FILETIME file_time{.dwLowDateTime = 0, .dwHighDateTime = 0};
  FileTimeToSystemTime(&file_time, &system_time);
  CHECK(system_time.wYear == 1601);
  CHECK(system_time.wMonth == 1);
  CHECK(system_time.wDay == 1);
  CHECK(system_time.wHour == 0);
  CHECK(system_time.wMinute == 0);
  CHECK(system_time.wSecond == 0);
  CHECK(system_time.wMilliseconds == 0);
}
#endif

TEST_CASE("Test clock resolutions", "[clockresolutiontests]") {
  CHECK(std::is_constructible<std::chrono::system_clock::duration, std::chrono::microseconds>::value);  // The resolution of the system_clock is at least microseconds
  CHECK(std::is_constructible<std::chrono::steady_clock::duration, std::chrono::microseconds>::value);  // The resolution of the steady_clock is at least microseconds
  CHECK(std::is_constructible<std::chrono::high_resolution_clock::duration, std::chrono::nanoseconds>::value);  // The resolution of the high_resolution_clock is at least nanoseconds
}

TEST_CASE("Test string to duration conversion", "[timedurationtests]") {
  using org::apache::nifi::minifi::utils::timeutils::StringToDuration;
  auto one_hour = StringToDuration<std::chrono::milliseconds>("1h");
  REQUIRE(one_hour);
  CHECK(one_hour.value() == 1h);
  CHECK(one_hour.value() == 3600s);

  CHECK(StringToDuration<std::chrono::milliseconds>("1 hour"));
  CHECK(StringToDuration<std::chrono::seconds>("102             hours") == 102h);
  CHECK(StringToDuration<std::chrono::days>("102             hours") == std::chrono::days(4));
  CHECK(StringToDuration<std::chrono::milliseconds>("5 ns") == 0ms);
  CHECK(StringToDuration<std::chrono::days>("2             weeks") == std::chrono::days(14));

  CHECK(StringToDuration<std::chrono::seconds>("1d") == std::chrono::days(1));
  CHECK(StringToDuration<std::chrono::seconds>("10 days") == std::chrono::days(10));
  CHECK(StringToDuration<std::chrono::seconds>("100ms") == 0ms);
  CHECK(StringToDuration<std::chrono::seconds>("20 us") == 0s);
  CHECK(StringToDuration<std::chrono::seconds>("1ns") == 0ns);
  CHECK(StringToDuration<std::chrono::seconds>("1min") == 1min);
  CHECK(StringToDuration<std::chrono::seconds>("1 hour") == 1h);
  CHECK(StringToDuration<std::chrono::seconds>("100 SEC") == 100s);
  CHECK(StringToDuration<std::chrono::seconds>("10 ms") == 0ms);
  CHECK(StringToDuration<std::chrono::seconds>("100 ns") == 0ns);
  CHECK(StringToDuration<std::chrono::seconds>("1 minute") == 1min);
  CHECK(StringToDuration<std::chrono::seconds>("1 w") == std::chrono::weeks(1));
  CHECK(StringToDuration<std::chrono::seconds>("3 weeks") == std::chrono::weeks(3));
  CHECK(StringToDuration<std::chrono::seconds>("2 months") == std::chrono::months(2));
  CHECK(StringToDuration<std::chrono::seconds>("1 y") == std::chrono::years(1));
  CHECK(StringToDuration<std::chrono::seconds>("2 years") == std::chrono::years(2));

  CHECK(StringToDuration<std::chrono::nanoseconds>("1d") == std::chrono::days(1));
  CHECK(StringToDuration<std::chrono::nanoseconds>("10 days") == std::chrono::days(10));
  CHECK(StringToDuration<std::chrono::nanoseconds>("100ms") == 100ms);
  CHECK(StringToDuration<std::chrono::nanoseconds>("20 us") == 20us);
  CHECK(StringToDuration<std::chrono::nanoseconds>("1ns") == 1ns);
  CHECK(StringToDuration<std::chrono::nanoseconds>("1min") == 1min);
  CHECK(StringToDuration<std::chrono::nanoseconds>("1 hour") == 1h);
  CHECK(StringToDuration<std::chrono::nanoseconds>("100 SEC") == 100s);
  CHECK(StringToDuration<std::chrono::nanoseconds>("10 ms") == 10ms);
  CHECK(StringToDuration<std::chrono::nanoseconds>("100 ns") == 100ns);
  CHECK(StringToDuration<std::chrono::nanoseconds>("1 minute") == 1min);
  CHECK(StringToDuration<std::chrono::nanoseconds>("1 w") == std::chrono::weeks(1));
  CHECK(StringToDuration<std::chrono::nanoseconds>("3 weeks") == std::chrono::weeks(3));
  CHECK(StringToDuration<std::chrono::nanoseconds>("2 months") == std::chrono::months(2));
  CHECK(StringToDuration<std::chrono::nanoseconds>("1 y") == std::chrono::years(1));
  CHECK(StringToDuration<std::chrono::nanoseconds>("2 years") == std::chrono::years(2));

  CHECK(StringToDuration<std::chrono::seconds>("5 apples") == std::nullopt);
  CHECK(StringToDuration<std::chrono::seconds>("20") == std::nullopt);
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

TEST_CASE("Parse RFC3339", "[parseRfc3339]") {
  using date::sys_days;
  using org::apache::nifi::minifi::utils::timeutils::parseRfc3339;
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;

  auto expected_second = sys_days(2023_y / 03 / 01) + 19h + 04min + 55s;
  auto expected_tenth_second = sys_days(2023_y / 03 / 01) + 19h + 04min + 55s + 100ms;
  auto expected_milli_second = sys_days(2023_y / 03 / 01) + 19h + 04min + 55s + 190ms;
  auto expected_micro_second = sys_days(2023_y / 03 / 01) + 19h + 04min + 55s + 190999us;

  CHECK(parseRfc3339("2023-03-01T19:04:55Z") == expected_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55.1Z") == expected_tenth_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55.19Z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55.190Z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55.190999Z") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01t19:04:55z") == expected_second);
  CHECK(parseRfc3339("2023-03-01t19:04:55.190z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01T20:04:55+01:00") == expected_second);
  CHECK(parseRfc3339("2023-03-01T20:04:55.190+01:00") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01T20:04:55.190999+01:00") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01 20:04:55+01:00") == expected_second);
  CHECK(parseRfc3339("2023-03-01 20:04:55.1+01:00") == expected_tenth_second);
  CHECK(parseRfc3339("2023-03-01 20:04:55.19+01:00") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01 20:04:55.190+01:00") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01 20:04:55.190999+01:00") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55Z") == expected_second);
  CHECK(parseRfc3339("2023-03-01_19:04:55Z") == expected_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55z") == expected_second);
  CHECK(parseRfc3339("2023-03-01_19:04:55z") == expected_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.1Z") == expected_tenth_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.19Z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.190Z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01_19:04:55.190Z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.190999Z") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01_19:04:55.190999Z") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.190z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01_19:04:55.190z") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.190999z") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01_19:04:55.190999z") == expected_micro_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55-00:00") == expected_second);
  CHECK(parseRfc3339("2023-03-01 19:04:55.190-00:00") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55-00:00") == expected_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55.190-00:00") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-02T03:49:55+08:45") == expected_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55+00:00") == expected_second);
  CHECK(parseRfc3339("2023-03-01T19:04:55.190+00:00") == expected_milli_second);
  CHECK(parseRfc3339("2023-03-01T18:04:55-01:00") == expected_second);

  CHECK_FALSE(parseRfc3339("2023-03-01T19:04:55Zbanana"));
  CHECK_FALSE(parseRfc3339("2023-03-01T19:04:55"));
  CHECK_FALSE(parseRfc3339("2023-03-01T19:04:55T"));
  CHECK_FALSE(parseRfc3339("2023-03-01T19:04:55Z "));
  CHECK_FALSE(parseRfc3339(" 2023-03-01T19:04:55Z"));
  CHECK_FALSE(parseRfc3339("2023-03-01"));
}

TEST_CASE("Test human readable parser") {
  using utils::timeutils::humanReadableDuration;
  CHECK(humanReadableDuration(1234567us) == "1.23s");
  const auto nine_hundred_forty_five_microseconds = humanReadableDuration(945us);
  CHECK((nine_hundred_forty_five_microseconds == "945.00µs" || nine_hundred_forty_five_microseconds == "945.00us"));
  CHECK(humanReadableDuration(52ms) == "52.00ms");

  CHECK(humanReadableDuration(1000s) == "00:16:40");
  CHECK(humanReadableDuration(10000s) == "02:46:40");
  CHECK(humanReadableDuration(2222222222ms) == "25 days, 17:17:02");
  CHECK(humanReadableDuration(24h) == "1 day, 00:00:00");
  CHECK(humanReadableDuration(23h + 12min + 1233ms) == "23:12:01");
}
