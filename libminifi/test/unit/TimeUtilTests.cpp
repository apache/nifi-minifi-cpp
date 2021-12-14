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

using namespace std::literals::chrono_literals;

namespace {
  constexpr int ONE_HOUR = 60 * 60;
  constexpr int ONE_DAY = 24 * ONE_HOUR;

  struct tm createTm(int year, int month, int day, int hour, int minute, int second, bool is_dst = false) {
    struct tm date_time;
    date_time.tm_year = year - 1900;
    date_time.tm_mon = month - 1;
    date_time.tm_mday = day;
    date_time.tm_hour = hour;
    date_time.tm_min = minute;
    date_time.tm_sec = second;
    date_time.tm_isdst = is_dst ? 1 : 0;
    return date_time;
  }

  void mkgmtimeTestHelper(time_t expected, int year, int month, int day, int hour, int minute, int second) {
    using org::apache::nifi::minifi::utils::timeutils::mkgmtime;
    struct tm date_time = createTm(year, month, day, hour, minute, second);
    REQUIRE(mkgmtime(&date_time) == expected);
  }
}  // namespace

TEST_CASE("mkgmtime() works correctly", "[mkgmtime]") {
  mkgmtimeTestHelper(0, 1970, 1, 1, 0, 0, 0);
  for (int hour = 0; hour < 24; ++hour) {
    mkgmtimeTestHelper((hour + 1) * ONE_HOUR - 1, 1970, 1, 1, hour, 59, 59);
  }

  mkgmtimeTestHelper(ONE_DAY,       1970, 1, 2, 0, 0, 0);
  mkgmtimeTestHelper(31 * ONE_DAY,  1970, 2, 1, 0, 0, 0);
  mkgmtimeTestHelper(365 * ONE_DAY, 1971, 1, 1, 0, 0, 0);

  mkgmtimeTestHelper(793929600,            1995, 2, 28, 0, 0, 0);
  mkgmtimeTestHelper(793929600 + ONE_DAY,  1995, 3,  1, 0, 0, 0);
  mkgmtimeTestHelper(825465600,            1996, 2, 28, 0, 0, 0);
  mkgmtimeTestHelper(825465600 + ONE_DAY,  1996, 2, 29, 0, 0, 0);
  mkgmtimeTestHelper(951696000,            2000, 2, 28, 0, 0, 0);
  mkgmtimeTestHelper(951696000 + ONE_DAY,  2000, 2, 29, 0, 0, 0);
  mkgmtimeTestHelper(4107456000,           2100, 2, 28, 0, 0, 0);
  mkgmtimeTestHelper(4107456000 + ONE_DAY, 2100, 3,  1, 0, 0, 0);

  mkgmtimeTestHelper(1513104856,  2017, 12, 12, 18, 54, 16);
  mkgmtimeTestHelper(1706655675,  2024,  1, 30, 23, 01, 15);
  mkgmtimeTestHelper(3710453630,  2087,  7, 31, 01, 33, 50);
}

TEST_CASE("parseDateTimeStr() works correctly", "[parseDateTimeStr]") {
  using org::apache::nifi::minifi::utils::timeutils::parseDateTimeStr;
  REQUIRE(parseDateTimeStr("1970-01-01T00:00:00Z") == 0);
  REQUIRE(parseDateTimeStr("1970-01-01T00:59:59Z") == ONE_HOUR - 1);

  REQUIRE(parseDateTimeStr("1970-01-02T00:00:00Z") == ONE_DAY);
  REQUIRE(parseDateTimeStr("1970-02-01T00:00:00Z") == 31 * ONE_DAY);
  REQUIRE(parseDateTimeStr("1971-01-01T00:00:00Z") == 365 * ONE_DAY);

  REQUIRE(parseDateTimeStr("1995-02-28T00:00:00Z") == 793929600);
  REQUIRE(parseDateTimeStr("1995-03-01T00:00:00Z") == 793929600 + ONE_DAY);
  REQUIRE(parseDateTimeStr("1996-02-28T00:00:00Z") == 825465600);
  REQUIRE(parseDateTimeStr("1996-02-29T00:00:00Z") == 825465600 + ONE_DAY);
  REQUIRE(parseDateTimeStr("2000-02-28T00:00:00Z") == 951696000);
  REQUIRE(parseDateTimeStr("2000-02-29T00:00:00Z") == 951696000 + ONE_DAY);
  REQUIRE(parseDateTimeStr("2100-02-28T00:00:00Z") == 4107456000);
  REQUIRE(parseDateTimeStr("2100-03-01T00:00:00Z") == 4107456000 + ONE_DAY);

  REQUIRE(parseDateTimeStr("2017-12-12T18:54:16Z") == 1513104856);
  REQUIRE(parseDateTimeStr("2024-01-30T23:01:15Z") == 1706655675);
  REQUIRE(parseDateTimeStr("2087-07-31T01:33:50Z") == 3710453630);
}

TEST_CASE("Test time conversion", "[testtimeconversion]") {
  using org::apache::nifi::minifi::utils::timeutils::getTimeStr;
  REQUIRE("2017-02-16 20:14:56.196" == getTimeStr(1487276096196, true));
}

TEST_CASE("Test system_clock epoch", "[systemclockepoch]") {
  using namespace std::chrono;
  time_point<system_clock> epoch;
  time_point<system_clock> unix_epoch_plus_3e9_sec = sys_days(January / 24 / 2065) + 5h + 20min;
  REQUIRE(epoch.time_since_epoch() == 0s);
  REQUIRE(unix_epoch_plus_3e9_sec.time_since_epoch() == 3000000000s);
}

TEST_CASE("Test clock resolutions", "[clockresolutiontests]") {
  using namespace std::chrono;
  CHECK(std::is_constructible<system_clock::duration, std::chrono::microseconds>::value);  // The resolution of the system_clock is at least microseconds
  CHECK(std::is_constructible<steady_clock::duration, std::chrono::microseconds>::value);  // The resolution of the system_clock is at least microseconds
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
