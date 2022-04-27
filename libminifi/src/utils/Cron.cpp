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

#include "utils/Cron.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "date/date.h"

using namespace std::literals::chrono_literals;

using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;
using std::chrono::days;

// TODO(C++20): move to std::chrono when calendar is fully supported
using date::local_seconds;
using date::day;
using date::weekday;
using date::month;
using date::year;
using date::year_month_day;
using date::last;
using date::local_days;
using date::from_stream;
using date::make_time;

using date::Friday; using date::Saturday; using date::Sunday;

namespace org::apache::nifi::minifi::utils {
namespace {

bool operator<=(const weekday& lhs, const weekday& rhs) {
  return lhs.c_encoding() <= rhs.c_encoding();
}

template <typename FieldType>
FieldType parse(const std::string&);

template <>
seconds parse<seconds>(const std::string& second_str) {
  auto sec_int = std::stoul(second_str);
  if (sec_int <= 59)
    return seconds(sec_int);
  throw BadCronExpression("Invalid second " + second_str);
}

template <>
minutes parse<minutes>(const std::string& minute_str) {
  auto min_int = std::stoul(minute_str);
  if (min_int <= 59)
    return minutes(min_int);
  throw BadCronExpression("Invalid minute " + minute_str);
}

template <>
hours parse<hours>(const std::string& hour_str) {
  auto hour_int = std::stoul(hour_str);
  if (hour_int <= 23)
    return hours(hour_int);
  throw BadCronExpression("Invalid hour " + hour_str);
}

template <>
days parse<days>(const std::string& days_str) {
  return days(std::stoul(days_str));
}

template <>
day parse<day>(const std::string& day_str) {
  auto day_int = std::stoul(day_str);
  if (day_int >= 1 && day_int <= 31)
    return day(day_int);
  throw BadCronExpression("Invalid day " + day_str);
}

template <>
month parse<month>(const std::string& month_str) {
// https://github.com/HowardHinnant/date/issues/550
// TODO(gcc11): Due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78714
// the month parsing with '%b' is case sensitive in gcc11
// This has been fixed in gcc12
#if defined(__GNUC__) && __GNUC__ < 12
  auto patched_month_str = StringUtils::toLower(month_str);
  if (!patched_month_str.empty())
    patched_month_str[0] = std::toupper(patched_month_str[0]);
  std::stringstream stream(patched_month_str);
#else
  std::stringstream stream(month_str);
#endif

  stream.imbue(std::locale("en_US.UTF-8"));
  month parsed_month{};
  if (month_str.size() > 2) {
    from_stream(stream, "%b", parsed_month);
    if (parsed_month.ok())
      return parsed_month;
  } else {
    from_stream(stream, "%m", parsed_month);
    if (parsed_month.ok())
      return parsed_month;
  }

  throw BadCronExpression("Invalid month " + month_str);
}

template <>
weekday parse<weekday>(const std::string& weekday_str) {
  std::stringstream stream(weekday_str);
  stream.imbue(std::locale("en_US.UTF-8"));

  if (weekday_str.size() > 2) {
    weekday parsed_weekday{};
    from_stream(stream, "%a", parsed_weekday);
    if (parsed_weekday.ok())
      return parsed_weekday;
  } else {
    unsigned weekday_num;
    stream >> weekday_num;
    if (!stream.bad() && weekday_num < 7)
      return weekday(weekday_num-1);
  }
  throw BadCronExpression("Invalid weekday: " + weekday_str);
}

template <>
year parse<year>(const std::string& year_str) {
  auto year_int = std::stoi(year_str);
  if (year_int >= 1970 && year_int <= 2099)
    return year(year_int);
  throw BadCronExpression("Invalid year: " + year_str);
}

template <typename FieldType>
FieldType getFieldType(local_seconds time_point);

template <>
year getFieldType(local_seconds time_point) {
  year_month_day year_month_day(floor<days>(time_point));
  return year_month_day.year();
}

template <>
month getFieldType(local_seconds time_point) {
  year_month_day year_month_day(floor<days>(time_point));
  return year_month_day.month();
}

template <>
day getFieldType(local_seconds time_point) {
  year_month_day year_month_day(floor<days>(time_point));
  return year_month_day.day();
}

template <>
hours getFieldType(local_seconds time_point) {
  auto dp = floor<days>(time_point);
  auto time = make_time(time_point-dp);
  return time.hours();
}

template <>
minutes getFieldType(local_seconds time_point) {
  auto dp = floor<days>(time_point);
  auto time = make_time(time_point-dp);
  return time.minutes();
}

template <>
seconds getFieldType(local_seconds time_point) {
  auto dp = floor<days>(time_point);
  auto time = make_time(time_point-dp);
  return time.seconds();
}

template <>
weekday getFieldType(local_seconds time_point) {
  auto dp = floor<days>(time_point);
  return weekday(dp);
}

bool isWeekday(year_month_day date) {
  weekday date_weekday = weekday(local_days(date));
  return date_weekday != Saturday && date_weekday != Sunday;
}

template <typename FieldType>
class SingleValueField : public CronField {
 public:
  explicit SingleValueField(FieldType value) : value_(value) {}

  [[nodiscard]] bool isValid(local_seconds time_point) const override {
    return value_ == getFieldType<FieldType>(time_point);
  }

 private:
  FieldType value_;
};

class NotCheckedField : public CronField {
 public:
  NotCheckedField() = default;

  [[nodiscard]] bool isValid(local_seconds) const override { return true; }
};

class AllValuesField : public CronField {
 public:
  AllValuesField() = default;

  [[nodiscard]] bool isValid(local_seconds) const override { return true; }
};

template <typename FieldType>
class RangeField : public CronField {
 public:
  explicit RangeField(FieldType lower_bound, FieldType upper_bound)
      : lower_bound_(std::move(lower_bound)),
        upper_bound_(std::move(upper_bound)) {
  }

  [[nodiscard]] bool isValid(local_seconds value) const override {
    return lower_bound_ <= getFieldType<FieldType>(value) && getFieldType<FieldType>(value) <= upper_bound_;
  }

 private:
  FieldType lower_bound_;
  FieldType upper_bound_;
};

template <typename FieldType>
class ListField : public CronField {
 public:
  explicit ListField(std::vector<FieldType> valid_values) : valid_values_(std::move(valid_values)) {}

  [[nodiscard]] bool isValid(local_seconds value) const override {
    return std::find(valid_values_.begin(), valid_values_.end(), getFieldType<FieldType>(value)) != valid_values_.end();
  }

 private:
  std::vector<FieldType> valid_values_;
};

template <typename FieldType>
class IncrementField : public CronField {
 public:
  IncrementField(FieldType start, int increment) : start_(start), increment_(increment) {}

  [[nodiscard]] bool isValid(local_seconds value) const override {
    return (getFieldType<FieldType>(value) - start_).count() % increment_ == 0;
  }

 private:
  FieldType start_;
  int increment_;
};

class LastNthDayInMonthField : public CronField {
 public:
  explicit LastNthDayInMonthField(days offset) : offset_(offset) {}

  [[nodiscard]] bool isValid(local_seconds tp) const override {
    year_month_day date(floor<days>(tp));
    auto last_day = date.year()/date.month()/last;
    auto target_date = local_days(last_day)-offset_;
    return local_days(date) == target_date;
  }

 private:
  days offset_;
};

class NthWeekdayField : public CronField {
 public:
  NthWeekdayField(weekday weekday, uint8_t n) : weekday_(weekday), n_(n) {}

  [[nodiscard]] bool isValid(local_seconds tp) const override {
    year_month_day date(floor<days>(tp));
    auto target_date = date.year()/date.month()/(weekday_[n_]);
    return local_days(date) == local_days(target_date);
  }

 private:
  weekday weekday_;
  uint8_t n_;
};

class LastWeekDayField : public CronField {
 public:
  LastWeekDayField() = default;

  [[nodiscard]] bool isValid(local_seconds value) const override {
    year_month_day date(floor<days>(value));
    year_month_day last_day_of_the_month_date = year_month_day(local_days(date.year()/date.month()/last));
    if (isWeekday(last_day_of_the_month_date))
      return date == last_day_of_the_month_date;
    year_month_day last_friday_of_the_month_date = year_month_day(local_days(date.year()/date.month()/Friday[last]));
    return date == last_friday_of_the_month_date;
  }
};

class ClosestWeekdayToX : public CronField {
 public:
  explicit ClosestWeekdayToX(day x) : x_(x) {}

  [[nodiscard]] bool isValid(local_seconds value) const override {
    year_month_day date(floor<days>(value));
    year_month_day target_date = year_month_day(local_days(date.year()/date.month()/x_));
    if (target_date.ok() && isWeekday(target_date))
      return target_date == date;

    target_date = year_month_day(local_days(date.year()/date.month()/(x_-days(1))));
    if (target_date.ok() && isWeekday(target_date))
      return target_date == date;

    target_date = year_month_day(local_days(date.year()/date.month()/(x_+days(1))));
    if (target_date.ok() && isWeekday(target_date))
      return target_date == date;

    target_date = year_month_day(local_days(date.year()/date.month()/(x_+days(2))));
    if (target_date.ok() && isWeekday(target_date))
      return target_date == date;

    return false;
  }

 private:
  day x_;
};

template <typename FieldType>
CronField* parseCronField(const std::string& field_str) {
  try {
    if (field_str == "*") {
      return new AllValuesField();
    }

    if (field_str == "?") {
      return new NotCheckedField();
    }

    if (field_str == "L") {
      if (std::is_same<day, FieldType>())
        return new LastNthDayInMonthField(days(0));
      if (std::is_same<weekday, FieldType>())
        return new SingleValueField(Saturday);
      throw BadCronExpression("L can only be used in the Day of month/Day of week fields");
    }

    if (field_str == "LW") {
      if (!std::is_same<day, FieldType>())
        throw BadCronExpression("LW can only be used in the Day of month field");
      return new LastWeekDayField();
    }

    if (field_str.find('L') != std::string::npos) {
      auto operands = StringUtils::split(field_str, "L");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
    }


    if (field_str.find('#') != std::string::npos) {
      if (!std::is_same<weekday, FieldType>())
        throw BadCronExpression("# can only be used in the Day of week field");
      auto operands = StringUtils::split(field_str, "#");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);

      return new NthWeekdayField(parse<weekday>(operands[0]), std::stoi(operands[1]));
    }

    if (field_str.find('-') != std::string::npos) {
      auto operands = StringUtils::split(field_str, "-");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
      if (operands[0] == "L") {
        if (std::is_same<day, FieldType>())
          return new LastNthDayInMonthField(parse<days>(operands[1]));
      }
      return new RangeField(parse<FieldType>(operands[0]), parse<FieldType>(operands[1]));
    }

    if (field_str.find('/') != std::string::npos) {
      auto operands = StringUtils::split(field_str, "/");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
      if (operands[0] == "*")
        operands[0] = "0";
      return new IncrementField(parse<FieldType>(operands[0]), std::stoi(operands[1]));
    }

    if (field_str.find(',') != std::string::npos) {
      auto operands_str = StringUtils::split(field_str, ",");
      std::vector<FieldType> operands;
      std::transform(operands_str.begin(), operands_str.end(), std::back_inserter(operands), parse<FieldType>);
      return new ListField(std::move(operands));
    }

    if (field_str.ends_with('W')) {
      if (!std::is_same<day, FieldType>())
        throw BadCronExpression("W can only be used in the Day of month field");
      auto operands_str = StringUtils::split(field_str, "W");
      if (operands_str.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
      return new ClosestWeekdayToX(parse<day>(operands_str[0]));
    }

    return new SingleValueField<FieldType>(parse<FieldType>(field_str));
  } catch (const std::exception& e) {
    throw BadCronExpression("Couldn't parse cron field: " + field_str + " " + e.what());
  }
}
}  // namespace

Cron::Cron(const std::string& expression) {
  auto tokens = StringUtils::split(expression, " ");

  if (tokens.size() != 6 && tokens.size() != 7)
    throw BadCronExpression("malformed cron string (must be 6 or 7 fields): " + expression);

  second_.reset(parseCronField<seconds>(tokens[0]));
  minute_.reset(parseCronField<minutes>(tokens[1]));
  hour_.reset(parseCronField<hours>(tokens[2]));
  day_.reset(parseCronField<day>(tokens[3]));
  month_.reset(parseCronField<month>(tokens[4]));
  day_of_week_.reset(parseCronField<weekday>(tokens[5]));
  if (tokens.size() == 7)
    year_.reset(parseCronField<year>(tokens[6]));
}

std::optional<local_seconds> Cron::calculateNextTrigger(const local_seconds start) const {
  gsl_Expects(second_ && minute_ && hour_ && day_ && month_ && day_of_week_);
  auto next = timeutils::roundToNextSecond(start);
  while (next < date::local_days((year(2999)/1/1))) {
    if (year_ && !year_->isValid(next)) {
      next = timeutils::roundToNextYear(next);
      continue;
    }
    if (!month_->isValid(next)) {
      next = timeutils::roundToNextMonth(next);
      continue;
    }
    if (!day_->isValid(next)) {
      next = timeutils::roundToNextDay(next);
      continue;
    }
    if (!day_of_week_->isValid(next)) {
      next = timeutils::roundToNextDay(next);
      continue;
    }
    if (!hour_->isValid(next)) {
      next = timeutils::roundToNextHour(next);
      continue;
    }
    if (!minute_->isValid(next)) {
      next = timeutils::roundToNextMinute(next);
      continue;
    }
    if (!second_->isValid(next)) {
      next = timeutils::roundToNextSecond(next);
      continue;
    }
    return next;
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils
