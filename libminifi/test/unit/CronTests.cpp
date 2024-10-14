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
#include "utils/Cron.h"
#include "date/date.h"
#include "date/tz.h"
#include "utils/TimeUtil.h"

using std::chrono::system_clock;
using std::chrono::seconds;
using org::apache::nifi::minifi::utils::Cron;
namespace timeutils = org::apache::nifi::minifi::utils::timeutils;


void checkNext(const std::string& expr, const date::zoned_time<seconds>& from, const date::zoned_time<seconds>& next) {
  auto cron_expression = Cron(expr);
  auto next_trigger = cron_expression.calculateNextTrigger(from.get_local_time());
  CHECK(next_trigger == next.get_local_time());
}

TEST_CASE("Cron expression ctor tests", "[cron]") {
  REQUIRE_THROWS(Cron("1600 ms"));
  REQUIRE_THROWS(Cron("foo"));
  REQUIRE_THROWS(Cron("61 0 0 * * *"));
  REQUIRE_THROWS(Cron("0 61 0 * * *"));
  REQUIRE_THROWS(Cron("0 0 24 * * *"));
  REQUIRE_THROWS(Cron("0 0 0 32 * *"));

  REQUIRE_THROWS(Cron("1banana * * * * * *"));
  REQUIRE_THROWS(Cron("* 1banana * * * * *"));
  REQUIRE_THROWS(Cron("* * 1banana * * * *"));
  REQUIRE_THROWS(Cron("* * * 1banana * * *"));
  REQUIRE_THROWS(Cron("* * * * 1banana * *"));
  REQUIRE_THROWS(Cron("* * * * DECbanana * *"));
  REQUIRE_THROWS(Cron("* * * * * WEDbanana *"));

  REQUIRE_THROWS(Cron("* * * * * * 1banana"));
  REQUIRE_THROWS(Cron("* * * * * * 2000banana"));

  REQUIRE_THROWS(Cron("1G * * * * * *"));
  REQUIRE_THROWS(Cron("* 1G * * * * *"));
  REQUIRE_THROWS(Cron("* * 1G * * * *"));
  REQUIRE_THROWS(Cron("* * * 1G * * *"));
  REQUIRE_THROWS(Cron("* * * * 1G * *"));
  REQUIRE_THROWS(Cron("* * * * * 1G *"));
  REQUIRE_THROWS(Cron("* * * * * * 1G"));

  // Number of fields must be 6 or 7
  REQUIRE_THROWS(Cron("* * * * *"));
  REQUIRE_NOTHROW(Cron("* * * * * *"));
  REQUIRE_NOTHROW(Cron("* * * * * * *"));
  REQUIRE_THROWS(Cron("* * * * * * * *"));

  // LW can only be used in 4th field
  REQUIRE_THROWS(Cron("LW * * * * * *"));
  REQUIRE_THROWS(Cron("* LW * * * * *"));
  REQUIRE_THROWS(Cron("* * LW * * * *"));
  REQUIRE_NOTHROW(Cron("* * * LW * * *"));
  REQUIRE_THROWS(Cron("* * * * LW * *"));
  REQUIRE_THROWS(Cron("* * * * * LW *"));
  REQUIRE_THROWS(Cron("* * * * * * LW"));

  // n#m can only be used in 6th field
  REQUIRE_THROWS(Cron("2#1 * * * * * *"));
  REQUIRE_THROWS(Cron("* 2#1 * * * * *"));
  REQUIRE_THROWS(Cron("* * 2#1 * * * *"));
  REQUIRE_THROWS(Cron("* * * 2#1 * * *"));
  REQUIRE_THROWS(Cron("* * * * 2#1 * *"));
  REQUIRE_NOTHROW(Cron("* * * * * 2#1 *"));
  REQUIRE_THROWS(Cron("* * * * * * 2#1"));

  // L can only be used in 4th, 6th fields
  REQUIRE_THROWS(Cron("L * * * * * *"));
  REQUIRE_THROWS(Cron("* L * * * * *"));
  REQUIRE_THROWS(Cron("* * L * * * *"));
  REQUIRE_NOTHROW(Cron("* * * L * * *"));
  REQUIRE_THROWS(Cron("* * * * L * *"));
  REQUIRE_NOTHROW(Cron("* * * * * L *"));
  REQUIRE_THROWS(Cron("* * * * * * L"));

  REQUIRE_NOTHROW(Cron("0 0 12 * * ?"));
  REQUIRE_NOTHROW(Cron("0 15 10 ? * *"));
  REQUIRE_NOTHROW(Cron("0 15 10 * * ?"));
  REQUIRE_NOTHROW(Cron("0 15 10 * * ? *"));
  REQUIRE_NOTHROW(Cron("0 15 10 * * ? 2005"));
  REQUIRE_NOTHROW(Cron("0 * 14 * * ?"));
  REQUIRE_NOTHROW(Cron("0 0/5 14 * * ?"));
  REQUIRE_NOTHROW(Cron("0 0/5 14,18 * * ?"));
  REQUIRE_NOTHROW(Cron("0 0-5 14 * * ?"));
  REQUIRE_NOTHROW(Cron("0 10,44 14 ? 3 WED"));
  REQUIRE_NOTHROW(Cron("0 15 10 ? * MON-FRI"));
  REQUIRE_NOTHROW(Cron("0 15 10 15 * ?"));
  REQUIRE_NOTHROW(Cron("0 15 10 L * ?"));
  REQUIRE_NOTHROW(Cron("0 15 10 L-2 * ?"));
  REQUIRE_NOTHROW(Cron("0 15 10 ? * 6L"));
  REQUIRE_NOTHROW(Cron("0 15 10 ? * 6L"));
  REQUIRE_NOTHROW(Cron("0 15 10 ? * 6L 2002-2005"));
  REQUIRE_NOTHROW(Cron("0 15 10 ? * 6#3"));
  REQUIRE_NOTHROW(Cron("0 0 12 1/5 * ?"));
  REQUIRE_NOTHROW(Cron("0 11 11 11 11 ?"));

  REQUIRE_THROWS(Cron("0 15 10 L-32 * ?"));
  REQUIRE_THROWS(Cron("15-10 * * * * * *"));
  REQUIRE_THROWS(Cron("* 4-3 * * * * *"));
  REQUIRE_THROWS(Cron("* * 4-3 * * * *"));
  REQUIRE_THROWS(Cron("* * * 31-29 * * *"));
  REQUIRE_THROWS(Cron("0 0 0 ? * MON-SUN"));
  REQUIRE_NOTHROW(Cron("0 0 0 ? * SUN-MON"));
}

TEST_CASE("Cron allowed nonnumerical inputs", "[cron]") {
  REQUIRE_NOTHROW(Cron("* * * * Jan,fEb,MAR,Apr,May,jun,Jul,Aug,Sep,Oct,Nov,Dec * *"));
  REQUIRE_NOTHROW(Cron("* * * * * Mon,tUe,WeD,Thu,Fri,SAT,Sun *"));
}

TEST_CASE("Day of the week checks", "[cron]") {
  auto monday = Cron("* * * * * MON");
  auto tuesday = Cron("* * * * * TUE");
  auto wednesday = Cron("* * * * * WED");
  auto thursday = Cron("* * * * * THU");
  auto friday = Cron("* * * * * FRI");
  auto saturday = Cron("* * * * * SAT");
  auto sunday = Cron("* * * * * SUN");

  auto day_zero = Cron("* * * * * 0");
  auto day_one = Cron("* * * * * 1");
  auto day_two = Cron("* * * * * 2");
  auto day_three = Cron("* * * * * 3");
  auto day_four = Cron("* * * * * 4");
  auto day_five = Cron("* * * * * 5");
  auto day_six = Cron("* * * * * 6");
  auto day_seven = Cron("* * * * * 7");

  CHECK(*sunday.day_of_week_ == *day_zero.day_of_week_);
  CHECK(*monday.day_of_week_ == *day_one.day_of_week_);
  CHECK(*tuesday.day_of_week_ == *day_two.day_of_week_);
  CHECK(*wednesday.day_of_week_ == *day_three.day_of_week_);
  CHECK(*thursday.day_of_week_ == *day_four.day_of_week_);
  CHECK(*friday.day_of_week_ == *day_five.day_of_week_);
  CHECK(*saturday.day_of_week_ == *day_six.day_of_week_);
  CHECK(*sunday.day_of_week_ == *day_seven.day_of_week_);
}

TEST_CASE("Cron::calculateNextTrigger", "[cron]") {
  using date::sys_days;
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
#ifdef WIN32
  timeutils::dateSetInstall(TZ_DATA_DIR);
#endif

  checkNext("0/15 * 1-4 * * ?",
            sys_days(2012_y / 07 / 01) + 9h + 53min + 50s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("0/15 * 1-4 * * ? *",
            sys_days(2012_y / 07 / 01) + 9h + 53min + 50s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("0/15 * 1-4 * * ?",
            sys_days(2012_y / 07 / 01) + 9h + 53min + 00s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("*/15 * 1-4 * * ?",
            sys_days(2012_y / 07 / 01) + 9h + 53min + 50s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("*/15 * 1-4 * * ? *",
            sys_days(2012_y / 07 / 01) + 9h + 53min + 50s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("*/15 * 1-4 * * ?",
            sys_days(2012_y / 07 / 01) + 9h + 53min + 00s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("0 0/2 1-4 * * ?",
            sys_days(2012_y / 07 / 01) + 9h + 00min + 00s,
            sys_days(2012_y / 07 / 02) + 01h + 00min + 00s);
  checkNext("* * * * * ?",
            sys_days(2012_y / 07 / 01) + 9h + 00min + 00s,
            sys_days(2012_y / 07 / 01) + 9h + 00min + 01s);
  checkNext("* * * * * ?",
            sys_days(2012_y / 12 / 01) + 9h + 00min + 58s,
            sys_days(2012_y / 12 / 01) + 9h + 00min + 59s);
  checkNext("10 * * * * ?",
            sys_days(2012_y / 12 / 01) + 9h + 42min + 9s,
            sys_days(2012_y / 12 / 01) + 9h + 42min + 10s);
  checkNext("11 * * * * ?",
            sys_days(2012_y / 12 / 01) + 9h + 42min + 10s,
            sys_days(2012_y / 12 / 01) + 9h + 42min + 11s);
  checkNext("10 * * * * ?",
            sys_days(2012_y / 12 / 01) + 9h + 42min + 10s,
            sys_days(2012_y / 12 / 01) + 9h + 43min + 10s);
  checkNext("10-15 * * * * ?",
            sys_days(2012_y / 12 / 01) + 9h + 42min + 9s,
            sys_days(2012_y / 12 / 01) + 9h + 42min + 10s);
  checkNext("10-15 * * * * ?",
            sys_days(2012_y / 12 / 01) + 21h + 42min + 14s,
            sys_days(2012_y / 12 / 01) + 21h + 42min + 15s);
  checkNext("0 * * * * ?",
            sys_days(2012_y / 12 / 01) + 21h + 10min + 42s,
            sys_days(2012_y / 12 / 01) + 21h + 11min + 00s);
  checkNext("0 * * * * ?",
            sys_days(2012_y / 12 / 01) + 21h + 11min + 00s,
            sys_days(2012_y / 12 / 01) + 21h + 12min + 00s);
  checkNext("0 11 * * * ?",
            sys_days(2012_y / 12 / 01) + 21h + 10min + 42s,
            sys_days(2012_y / 12 / 01) + 21h + 11min + 00s);
  checkNext("0 10 * * * ?",
            sys_days(2012_y / 12 / 01) + 21h + 11min + 00s,
            sys_days(2012_y / 12 / 01) + 22h + 10min + 00s);
  checkNext("0 0 * * * ?",
            sys_days(2012_y / 9 / 30) + 11h + 01min + 00s,
            sys_days(2012_y / 9 / 30) + 12h + 00min + 00s);
  checkNext("0 0 * * * ?",
            sys_days(2012_y / 9 / 30) + 12h + 00min + 00s,
            sys_days(2012_y / 9 / 30) + 13h + 00min + 00s);
  checkNext("0 0 * * * ?",
            sys_days(2012_y / 9 / 10) + 23h + 01min + 00s,
            sys_days(2012_y / 9 / 11) + 00h + 00min + 00s);
  checkNext("0 0 * * * ?",
            sys_days(2012_y / 9 / 11) + 00h + 00min + 00s,
            sys_days(2012_y / 9 / 11) + 01h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 9 / 01) + 14h + 42min + 43s,
            sys_days(2012_y / 9 / 02) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 9 / 02) + 00h + 00min + 00s,
            sys_days(2012_y / 9 / 03) + 00h + 00min + 00s);
  checkNext("* * * 10 * ?",
            sys_days(2012_y / 10 / 9) + 15h + 12min + 42s,
            sys_days(2012_y / 10 / 10) + 00h + 00min + 00s);
  checkNext("* * * 10 * ?",
            sys_days(2012_y / 10 / 11) + 15h + 12min + 42s,
            sys_days(2012_y / 11 / 10) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ? 2020",
            sys_days(2012_y / 9 / 30) + 15h + 12min + 42s,
            sys_days(2020_y / 01 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 9 / 30) + 15h + 12min + 42s,
            sys_days(2012_y / 10 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 10 / 01) + 00h + 00min + 00s,
            sys_days(2012_y / 10 / 02) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 8 / 30) + 15h + 12min + 42s,
            sys_days(2012_y / 8 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 8 / 31) + 00h + 00min + 00s,
            sys_days(2012_y / 9 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 10 / 30) + 15h + 12min + 42s,
            sys_days(2012_y / 10 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 * * ?",
            sys_days(2012_y / 10 / 31) + 00h + 00min + 00s,
            sys_days(2012_y / 11 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 1 * ?",
            sys_days(2012_y / 10 / 30) + 15h + 12min + 42s,
            sys_days(2012_y / 11 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 1 * ?",
            sys_days(2012_y / 11 / 01) + 00h + 00min + 00s,
            sys_days(2012_y / 12 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 1 * ?",
            sys_days(2010_y / 12 / 31) + 15h + 12min + 42s,
            sys_days(2011_y / 01 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 1 * ?",
            sys_days(2011_y / 01 / 01) + 00h + 00min + 00s,
            sys_days(2011_y / 02 / 01) + 00h + 00min + 00s);
  checkNext("0 0 0 31 * ?",
            sys_days(2011_y / 10 / 30) + 15h + 12min + 42s,
            sys_days(2011_y / 10 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 1 * ?",
            sys_days(2011_y / 10 / 30) + 15h + 12min + 42s,
            sys_days(2011_y / 11 / 01) + 00h + 00min + 00s);
  checkNext("* * * ? * 1",
            sys_days(2010_y / 10 / 25) + 15h + 12min + 42s,
            sys_days(2010_y / 10 / 25) + 15h + 12min + 43s);
  checkNext("* * * ? * 1",
            sys_days(2010_y / 10 / 20) + 15h + 12min + 42s,
            sys_days(2010_y / 10 / 25) + 00h + 00min + 00s);
  checkNext("* * * ? * 1",
            sys_days(2010_y / 10 / 27) + 15h + 12min + 42s,
            sys_days(2010_y / 11 / 01) + 00h + 00min + 00s);
  checkNext("55 5 * * * ?",
            sys_days(2010_y / 10 / 27) + 15h + 04min + 54s,
            sys_days(2010_y / 10 / 27) + 15h + 05min + 55s);
  checkNext("55 5 * * * ?",
            sys_days(2010_y / 10 / 27) + 15h + 05min + 55s,
            sys_days(2010_y / 10 / 27) + 16h + 05min + 55s);
  checkNext("55 * 10 * * ?",
            sys_days(2010_y / 10 / 27) + 9h + 04min + 54s,
            sys_days(2010_y / 10 / 27) + 10h + 00min + 55s);
  checkNext("55 * 10 * * ?",
            sys_days(2010_y / 10 / 27) + 10h + 00min + 55s,
            sys_days(2010_y / 10 / 27) + 10h + 01min + 55s);
  checkNext("* 5 10 * * ?",
            sys_days(2010_y / 10 / 27) + 9h + 04min + 55s,
            sys_days(2010_y / 10 / 27) + 10h + 05min + 00s);
  checkNext("* 5 10 * * ?",
            sys_days(2010_y / 10 / 27) + 10h + 05min + 00s,
            sys_days(2010_y / 10 / 27) + 10h + 05min + 01s);
  checkNext("55 * * 3 * ?",
            sys_days(2010_y / 10 / 02) + 10h + 05min + 54s,
            sys_days(2010_y / 10 / 03) + 00h + 00min + 55s);
  checkNext("55 * * 3 * ?",
            sys_days(2010_y / 10 / 03) + 00h + 00min + 55s,
            sys_days(2010_y / 10 / 03) + 00h + 01min + 55s);
  checkNext("* * * 3 11 ?",
            sys_days(2010_y / 10 / 02) + 14h + 42min + 55s,
            sys_days(2010_y / 11 / 03) + 00h + 00min + 00s);
  checkNext("* * * 3 11 ?",
            sys_days(2010_y / 11 / 03) + 00h + 00min + 00s,
            sys_days(2010_y / 11 / 03) + 00h + 00min + 01s);
  checkNext("0 0 0 29 2 ?",
            sys_days(2007_y / 02 / 10) + 14h + 42min + 55s,
            sys_days(2008_y / 02 / 29) + 00h + 00min + 00s);
  checkNext("0 0 0 29 2 ?",
            sys_days(2008_y / 02 / 29) + 00h + 00min + 00s,
            sys_days(2012_y / 02 / 29) + 00h + 00min + 00s);
  checkNext("0 0 7 ? * Mon-Fri",
            sys_days(2009_y / 9 / 26) + 00h + 42min + 55s,
            sys_days(2009_y / 9 / 28) + 07h + 00min + 00s);
  checkNext("0 0 7 ? * Mon-Fri",
            sys_days(2009_y / 9 / 26) + 00h + 42min + 55s,
            sys_days(2009_y / 9 / 28) + 07h + 00min + 00s);
  checkNext("0 0 7 ? * Mon,Tue,Wed,Thu,Fri",
            sys_days(2009_y / 9 / 28) + 07h + 00min + 00s,
            sys_days(2009_y / 9 / 29) + 07h + 00min + 00s);
  checkNext("0 30 23 30 1/3 ?",
            sys_days(2010_y / 12 / 30) + 00h + 00min + 00s,
            sys_days(2011_y / 01 / 30) + 23h + 30min + 00s);
  checkNext("0 30 23 30 1/3 ?",
            sys_days(2011_y / 01 / 30) + 23h + 30min + 00s,
            sys_days(2011_y / 04 / 30) + 23h + 30min + 00s);
  checkNext("0 30 23 30 1/3 ?",
            sys_days(2011_y / 04 / 30) + 23h + 30min + 00s,
            sys_days(2011_y / 07 / 30) + 23h + 30min + 00s);

  checkNext("0 0 0 LW * ? *",
            sys_days(2022_y / 02 / 27) + 02h + 00min + 00s,
            sys_days(2022_y / 02 / 28) + 00h + 00min + 00s);
  checkNext("0 0 0 LW * ? *",
            sys_days(2024_y / 02 / 27) + 02h + 00min + 00s,
            sys_days(2024_y / 02 / 29) + 00h + 00min + 00s);
  checkNext("0 0 0 LW * ? *",
            sys_days(2027_y / 02 / 27) + 02h + 00min + 00s,
            sys_days(2027_y / 03 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * 2#1 *",
            sys_days(2022_y / 05 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 06 / 07) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * 2#2 *",
            sys_days(2022_y / 05 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 10) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * 2#3 *",
            sys_days(2022_y / 05 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 17) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * 2#4 *",
            sys_days(2022_y / 05 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 24) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * 2#5 *",
            sys_days(2022_y / 05 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 L * ? *",
            sys_days(2022_y / 01 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 01 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 L * ? *",
            sys_days(2022_y / 02 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 02 / 28) + 00h + 00min + 00s);
  checkNext("0 0 0 L * ? *",
            sys_days(2024_y / 02 / 04) + 00h + 00min + 00s,
            sys_days(2024_y / 02 / 29) + 00h + 00min + 00s);
  checkNext("0 0 0 L * ? *",
            sys_days(2022_y / 03 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 03 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 L * ? *",
            sys_days(2022_y / 04 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 04 / 30) + 00h + 00min + 00s);
  checkNext("0 0 0 L * ? *",
            sys_days(2022_y / 05 / 31) + 00h + 00min + 00s,
            sys_days(2022_y / 06 / 30) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * L *",
            sys_days(2022_y / 01 / 07) + 00h + 00min + 00s,
            sys_days(2022_y / 01 / 8) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * L *",
            sys_days(2022_y / 02 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 02 / 05) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * L *",
            sys_days(2024_y / 02 / 04) + 00h + 00min + 00s,
            sys_days(2024_y / 02 / 10) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * L *",
            sys_days(2022_y / 03 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 03 / 05) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * L *",
            sys_days(2022_y / 04 / 04) + 00h + 00min + 00s,
            sys_days(2022_y / 04 / 9) + 00h + 00min + 00s);
  checkNext("0 0 0 ? * L *",
            sys_days(2022_y / 05 / 28) + 00h + 00min + 00s,
            sys_days(2022_y / 06 / 04) + 00h + 00min + 00s);
  checkNext("0 0 0 1W * ? *",
            sys_days(2022_y / 05 / 01) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 02) + 00h + 00min + 00s);
  checkNext("0 0 0 4W * ? *",
            sys_days(2022_y / 05 / 01) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 04) + 00h + 00min + 00s);
  checkNext("0 0 0 14W * ? *",
            sys_days(2022_y / 05 / 01) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 13) + 00h + 00min + 00s);
  checkNext("0 0 0 15W * ? *",
            sys_days(2022_y / 05 / 01) + 00h + 00min + 00s,
            sys_days(2022_y / 05 / 16) + 00h + 00min + 00s);
  checkNext("0 0 0 31W * ? *",
            sys_days(2022_y / 02 / 01) + 00h + 00min + 00s,
            sys_days(2022_y / 03 / 31) + 00h + 00min + 00s);
  checkNext("0 0 0 1W * ? *",
            sys_days(2021_y / 12 / 15) + 00h + 00min + 00s,
            sys_days(2022_y / 01 / 03) + 00h + 00min + 00s);
  checkNext("0 0 0 31W * ? *",
            sys_days(2022_y / 07 / 15) + 00h + 00min + 00s,
            sys_days(2022_y / 07 / 29) + 00h + 00min + 00s);

  checkNext("0 15 10 ? * 5L",
            sys_days(2022_y / 07 / 15) + 00h + 00min + 00s,
            sys_days(2022_y / 07 / 29) + 10h + 15min + 00s);

  checkNext("0 0 0 L-3 * ?",
            sys_days(2022_y / 01 / 10) + 00h + 00min + 00s,
            sys_days(2022_y / 01 / 28) + 00h + 00min + 00s);

  checkNext("0 0 0 L-30 * ?",
            sys_days(2022_y / 01 / 10) + 00h + 00min + 00s,
            sys_days(2022_y / 03 / 01) + 00h + 00min + 00s);
}

TEST_CASE("Cron::calculateNextTrigger with timezones", "[cron]") {
  using date::local_days;
  using date::locate_zone;
  using date::zoned_time;
  using namespace date::literals;  // NOLINT(google-build-using-namespace)
  using namespace std::literals::chrono_literals;
#ifdef WIN32
  timeutils::dateSetInstall(TZ_DATA_DIR);
#endif

  const std::vector<std::string> time_zones{ "Europe/Berlin", "Asia/Seoul", "America/Los_Angeles", "Asia/Singapore", "UTC" };

  for (const auto& time_zone: time_zones) {
    checkNext("0/15 * 1-4 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 53min + 50s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("0/15 * 1-4 * * ? *",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 53min + 50s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("0/15 * 1-4 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 53min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("0/15 * 1-4 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 53min + 50s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("0/15 * 1-4 * * ? *",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 53min + 50s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("0/15 * 1-4 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 53min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("0 0/2 1-4 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 02) + 01h + 00min + 00s));
    checkNext("* * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 07 / 01) + 9h + 00min + 01s));
    checkNext("* * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 00min + 58s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 00min + 59s));
    checkNext("10 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 9s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 10s));
    checkNext("11 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 10s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 11s));
    checkNext("10 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 10s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 43min + 10s));
    checkNext("10-15 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 9s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 9h + 42min + 10s));
    checkNext("10-15 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 42min + 14s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 42min + 15s));
    checkNext("0 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 10min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 11min + 00s));
    checkNext("0 * * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 11min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 12min + 00s));
    checkNext("0 11 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 10min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 11min + 00s));
    checkNext("0 10 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 21h + 11min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2016_y / 12 / 01) + 22h + 10min + 00s));
    checkNext("0 0 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 30) + 11h + 01min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 30) + 12h + 00min + 00s));
    checkNext("0 0 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 30) + 12h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 30) + 13h + 00min + 00s));
    checkNext("0 0 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 10) + 23h + 01min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 11) + 00h + 00min + 00s));
    checkNext("0 0 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 11) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 11) + 01h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 01) + 14h + 42min + 43s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 02) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 02) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 03) + 00h + 00min + 00s));
    checkNext("* * * 10 * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 9) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 10) + 00h + 00min + 00s));
    checkNext("* * * 10 * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 11) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 11 / 10) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ? 2020",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2020_y / 01 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 02) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 8 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 8 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 8 / 31) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 9 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 31) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 11 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 1 * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 10 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 11 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 1 * ?",
              make_zoned(locate_zone(time_zone), local_days(2012_y / 11 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 12 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 1 * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 12 / 31) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 01 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 1 * ?",
              make_zoned(locate_zone(time_zone), local_days(2011_y / 01 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 02 / 01) + 00h + 00min + 00s));
    checkNext("0 0 0 31 * ?",
              make_zoned(locate_zone(time_zone), local_days(2011_y / 10 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 10 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 1 * ?",
              make_zoned(locate_zone(time_zone), local_days(2011_y / 10 / 30) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 11 / 01) + 00h + 00min + 00s));
    checkNext("* * * ? * 1",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 25) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 25) + 15h + 12min + 43s));
    checkNext("* * * ? * 1",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 20) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 25) + 00h + 00min + 00s));
    checkNext("* * * ? * 1",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 15h + 12min + 42s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 11 / 01) + 00h + 00min + 00s));
    checkNext("55 5 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 15h + 04min + 54s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 15h + 05min + 55s));
    checkNext("55 5 * * * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 15h + 05min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 16h + 05min + 55s));
    checkNext("55 * 10 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 9h + 04min + 54s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 10h + 00min + 55s));
    checkNext("55 * 10 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 10h + 00min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 10h + 01min + 55s));
    checkNext("* 5 10 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 9h + 04min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 10h + 05min + 00s));
    checkNext("* 5 10 * * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 10h + 05min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 27) + 10h + 05min + 01s));
    checkNext("55 * * 3 * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 02) + 10h + 05min + 54s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 03) + 00h + 00min + 55s));
    checkNext("55 * * 3 * ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 03) + 00h + 00min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 03) + 00h + 01min + 55s));
    checkNext("* * * 3 11 ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 10 / 02) + 14h + 42min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 11 / 03) + 00h + 00min + 00s));
    checkNext("* * * 3 11 ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 11 / 03) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2010_y / 11 / 03) + 00h + 00min + 01s));
    checkNext("0 0 0 29 2 ?",
              make_zoned(locate_zone(time_zone), local_days(2007_y / 02 / 10) + 14h + 42min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2008_y / 02 / 29) + 00h + 00min + 00s));
    checkNext("0 0 0 29 2 ?",
              make_zoned(locate_zone(time_zone), local_days(2008_y / 02 / 29) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2012_y / 02 / 29) + 00h + 00min + 00s));
    checkNext("0 0 7 ? * Mon-Fri",
              make_zoned(locate_zone(time_zone), local_days(2009_y / 9 / 26) + 00h + 42min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2009_y / 9 / 28) + 07h + 00min + 00s));
    checkNext("0 0 7 ? * Mon-Fri",
              make_zoned(locate_zone(time_zone), local_days(2009_y / 9 / 26) + 00h + 42min + 55s),
              make_zoned(locate_zone(time_zone), local_days(2009_y / 9 / 28) + 07h + 00min + 00s));
    checkNext("0 0 7 ? * Mon,Tue,Wed,Thu,Fri",
              make_zoned(locate_zone(time_zone), local_days(2009_y / 9 / 28) + 07h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2009_y / 9 / 29) + 07h + 00min + 00s));
    checkNext("0 30 23 30 1/3 ?",
              make_zoned(locate_zone(time_zone), local_days(2010_y / 12 / 30) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 01 / 30) + 23h + 30min + 00s));
    checkNext("0 30 23 30 1/3 ?",
              make_zoned(locate_zone(time_zone), local_days(2011_y / 01 / 30) + 23h + 30min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 04 / 30) + 23h + 30min + 00s));
    checkNext("0 30 23 30 1/3 ?",
              make_zoned(locate_zone(time_zone), local_days(2011_y / 04 / 30) + 23h + 30min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2011_y / 07 / 30) + 23h + 30min + 00s));

    checkNext("0 0 0 LW * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 27) + 02h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 28) + 00h + 00min + 00s));
    checkNext("0 0 0 LW * ? *",
              make_zoned(locate_zone(time_zone), local_days(2024_y / 02 / 27) + 02h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2024_y / 02 / 29) + 00h + 00min + 00s));
    checkNext("0 0 0 LW * ? *",
              make_zoned(locate_zone(time_zone), local_days(2027_y / 02 / 27) + 02h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2027_y / 03 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * 2#1 *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 06 / 07) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * 2#2 *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 10) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * 2#3 *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 17) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * 2#4 *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 24) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * 2#5 *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 L * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 L * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 28) + 00h + 00min + 00s));
    checkNext("0 0 0 L * ? *",
              make_zoned(locate_zone(time_zone), local_days(2024_y / 02 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2024_y / 02 / 29) + 00h + 00min + 00s));
    checkNext("0 0 0 L * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 03 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 03 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 L * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 04 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 04 / 30) + 00h + 00min + 00s));
    checkNext("0 0 0 L * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 31) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 06 / 30) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * L *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 07) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 8) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * L *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 05) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * L *",
              make_zoned(locate_zone(time_zone), local_days(2024_y / 02 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2024_y / 02 / 10) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * L *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 03 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 03 / 05) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * L *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 04 / 04) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 04 / 9) + 00h + 00min + 00s));
    checkNext("0 0 0 ? * L *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 28) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 06 / 04) + 00h + 00min + 00s));
    checkNext("0 0 0 1W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 02) + 00h + 00min + 00s));
    checkNext("0 0 0 4W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 04) + 00h + 00min + 00s));
    checkNext("0 0 0 14W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 13) + 00h + 00min + 00s));
    checkNext("0 0 0 15W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 05 / 16) + 00h + 00min + 00s));
    checkNext("0 0 0 31W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 02 / 01) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 03 / 31) + 00h + 00min + 00s));
    checkNext("0 0 0 1W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2021_y / 12 / 15) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 03) + 00h + 00min + 00s));
    checkNext("0 0 0 31W * ? *",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 07 / 15) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 07 / 29) + 00h + 00min + 00s));

    checkNext("0 15 10 ? * 5L",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 07 / 15) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 07 / 29) + 10h + 15min + 00s));

    checkNext("0 0 0 L-3 * ?",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 10) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 28) + 00h + 00min + 00s));

    checkNext("0 0 0 L-30 * ?",
              make_zoned(locate_zone(time_zone), local_days(2022_y / 01 / 10) + 00h + 00min + 00s),
              make_zoned(locate_zone(time_zone), local_days(2022_y / 03 / 01) + 00h + 00min + 00s));
  }
}
