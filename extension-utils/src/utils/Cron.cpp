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
#include <charconv>
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "date/date.h"

using namespace std::literals::chrono_literals;

using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;
using std::chrono::days;

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
using date::Friday;
using date::Saturday;
using date::Sunday;

namespace org::apache::nifi::minifi::utils {
namespace {

// https://github.com/HowardHinnant/date/issues/550
// Due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=78714
// the month parsing with '%b' and the weekday parsing with '%a' is case-sensitive in gcc11
// This has been fixed in gcc13
std::stringstream getCaseInsensitiveCStream(const std::string& str) {
#if defined(__GNUC__) && (__GNUC__ < 13)
  auto patched_str = string::toLower(str);
  if (!patched_str.empty())
    patched_str[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(patched_str[0])));
  auto stream = std::stringstream{patched_str};
#else
  auto stream = std::stringstream{str};
#endif
  stream.imbue(std::locale::classic());
  return stream;
}


template<class T>
std::optional<T> fromChars(const std::string& input) {
  T t{};
  const auto result = std::from_chars(input.data(), input.data() + input.size(), t);
  if (result.ptr != input.data() + input.size())
    return std::nullopt;
  return t;
}

bool operator<=(const weekday& lhs, const weekday& rhs) {
  return lhs.c_encoding() <= rhs.c_encoding();
}

template <typename FieldType>
FieldType parse(const std::string&);

template <>
seconds parse<seconds>(const std::string& second_str) {
  if (auto sec_int = fromChars<uint64_t>(second_str); sec_int && *sec_int <= 59)
    return seconds(*sec_int);
  throw BadCronExpression("Invalid second " + second_str);
}

template <>
minutes parse<minutes>(const std::string& minute_str) {
  if (auto min_int = fromChars<uint64_t>(minute_str); min_int && *min_int <= 59)
    return minutes(*min_int);
  throw BadCronExpression("Invalid minute " + minute_str);
}

template <>
hours parse<hours>(const std::string& hour_str) {
  if (auto hour_int = fromChars<uint64_t>(hour_str); hour_int && *hour_int <= 23)
    return hours(*hour_int);
  throw BadCronExpression("Invalid hour " + hour_str);
}

template <>
days parse<days>(const std::string& days_str) {
  if (auto days_int = fromChars<uint64_t>(days_str))
    return days(*days_int);
  throw BadCronExpression("Invalid days " + days_str);
}

template <>
day parse<day>(const std::string& day_str) {
  if (auto day_int = fromChars<unsigned int>(day_str); day_int && day_int.value() >= 1 && day_int.value() <= 31)
    return day(*day_int);
  throw BadCronExpression("Invalid day " + day_str);
}

template <>
month parse<month>(const std::string& month_str) {
  std::stringstream stream = getCaseInsensitiveCStream(month_str);

  month parsed_month{};
  if (month_str.size() > 2) {
    from_stream(stream, "%b", parsed_month);
    if (!stream.fail() && parsed_month.ok() && stream.peek() == EOF)
      return parsed_month;
  } else {
    from_stream(stream, "%m", parsed_month);
    if (!stream.fail() && parsed_month.ok() && stream.peek() == EOF)
      return parsed_month;
  }

  throw BadCronExpression("Invalid month " + month_str);
}

template <>
weekday parse<weekday>(const std::string& weekday_str) {
  std::stringstream stream = getCaseInsensitiveCStream(weekday_str);

  if (weekday_str.size() > 2) {
    weekday parsed_weekday{};
    from_stream(stream, "%a", parsed_weekday);
    if (!stream.fail() && parsed_weekday.ok() && stream.peek() == EOF)
      return parsed_weekday;
  } else {
    unsigned weekday_num = 0;
    stream >> weekday_num;
    if (!stream.fail() && weekday_num < 8 && stream.peek() == EOF)
      return weekday(weekday_num);
  }
  throw BadCronExpression("Invalid weekday: " + weekday_str);
}

template <>
year parse<year>(const std::string& year_str) {
  if (auto year_int = fromChars<int>(year_str); year_int && *year_int >= 1970 && *year_int <= 2999)
    return year(*year_int);
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

  [[nodiscard]] bool matches(local_seconds time_point) const override {
    return value_ == getFieldType<FieldType>(time_point);
  }

  bool operator==(const CronField& rhs) const override {
    if (auto rhs_single_value = dynamic_cast<const SingleValueField*>(&rhs)) {
      return value_ == rhs_single_value->value_;
    }
    return false;
  }


 private:
  FieldType value_;
};

class NotCheckedField : public CronField {
 public:
  NotCheckedField() = default;

  [[nodiscard]] bool matches(local_seconds) const override { return true; }
};

class AllValuesField : public CronField {
 public:
  AllValuesField() = default;

  [[nodiscard]] bool matches(local_seconds) const override { return true; }
};

template <typename FieldType>
class RangeField : public CronField {
 public:
  explicit RangeField(FieldType lower_bound, FieldType upper_bound)
      : lower_bound_(std::move(lower_bound)),
        upper_bound_(std::move(upper_bound)) {
    if (!(lower_bound_ <= upper_bound_))
      throw std::out_of_range("lower bound must be smaller or equal to upper bound");
  }

  [[nodiscard]] bool matches(local_seconds value) const override {
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

  [[nodiscard]] bool matches(local_seconds value) const override {
    return std::find(valid_values_.begin(), valid_values_.end(), getFieldType<FieldType>(value)) != valid_values_.end();
  }

 private:
  std::vector<FieldType> valid_values_;
};

template <typename FieldType>
class IncrementField : public CronField {
 public:
  IncrementField(FieldType start, int increment) : start_(start), increment_(increment) {}

  [[nodiscard]] bool matches(local_seconds value) const override {
    return (getFieldType<FieldType>(value) - start_).count() % increment_ == 0;
  }

 private:
  FieldType start_;
  int increment_;
};

class LastNthDayInMonthField : public CronField {
 public:
  explicit LastNthDayInMonthField(days offset) : offset_(offset) {
    if (!(offset_ <= std::chrono::days(30)))
      throw BadCronExpression("Offset from last day must be <= 30");
  }

  [[nodiscard]] bool matches(local_seconds tp) const override {
    year_month_day date(floor<days>(tp));
    auto last_day = date.year() / date.month() / last;
    auto target_date = local_days(last_day) - offset_;
    return local_days(date) == target_date;
  }

 private:
  days offset_;
};

class NthWeekdayField : public CronField {
 public:
  NthWeekdayField(weekday wday, uint8_t n) : weekday_(wday), n_(n) {}

  [[nodiscard]] bool matches(local_seconds tp) const override {
    year_month_day date(floor<days>(tp));
    auto target_date = date.year() / date.month() / (weekday_[n_]);
    return local_days(date) == local_days(target_date);
  }

 private:
  weekday weekday_;
  uint8_t n_;
};

class LastWeekDayField : public CronField {
 public:
  LastWeekDayField() = default;

  [[nodiscard]] bool matches(local_seconds value) const override {
    year_month_day date(floor<days>(value));
    year_month_day last_day_of_the_month_date = year_month_day(local_days(date.year()/date.month()/last));
    if (isWeekday(last_day_of_the_month_date))
      return date == last_day_of_the_month_date;
    year_month_day last_friday_of_the_month_date = year_month_day(local_days(date.year()/date.month()/Friday[last]));
    return date == last_friday_of_the_month_date;
  }
};

class LastSpecificDayOfTheWeekOfTheMonth : public CronField {
 public:
  explicit LastSpecificDayOfTheWeekOfTheMonth(weekday wday) : weekday_(wday) {}

  [[nodiscard]] bool matches(local_seconds value) const override {
    year_month_day date(floor<days>(value));
    year_month_day last_weekday_of_month_date = year_month_day(local_days(date.year()/date.month()/weekday_[last]));
    return date == last_weekday_of_month_date;
  }
 private:
  weekday weekday_;
};

class ClosestWeekdayToTheNthDayOfTheMonth : public CronField {
 public:
  explicit ClosestWeekdayToTheNthDayOfTheMonth(day day_number) : day_number_(day_number) {}

  [[nodiscard]] bool matches(local_seconds value) const override {
    year_month_day date(floor<days>(value));
    for (auto diff : {0, -1, 1, -2, 2}) {
      auto target_date = date.year() / date.month() / (day_number_ + days(diff));
      if (target_date.ok() && isWeekday(target_date))
        return target_date == date;
    }

    return false;
  }

 private:
  day day_number_;
};

template <typename FieldType>
std::unique_ptr<CronField> parseCronField(const std::string& field_str) {
  try {
    if (field_str == "*") {
      return std::make_unique<AllValuesField>();
    }

    if (field_str == "?") {
      return std::make_unique<NotCheckedField>();
    }

    if (field_str == "L") {
      if constexpr (std::is_same<day, FieldType>())
        return std::make_unique<LastNthDayInMonthField>(days(0));
      if constexpr (std::is_same<weekday, FieldType>())
        return std::make_unique<SingleValueField<weekday>>(Saturday);
      throw BadCronExpression("L can only be used in the Day of month/Day of week fields");
    }

    if (field_str == "LW") {
      if constexpr (!std::is_same<day, FieldType>())
        throw BadCronExpression("LW can only be used in the Day of month field");
      return std::make_unique<LastWeekDayField>();
    }

    if (field_str.find('#') != std::string::npos) {
      if constexpr (!std::is_same<weekday, FieldType>())
        throw BadCronExpression("# can only be used in the Day of week field");
      auto operands = string::split(field_str, "#");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);

      if (auto second_operand = fromChars<uint8_t>(operands[1]))
        return std::make_unique<NthWeekdayField>(parse<weekday>(operands[0]), *second_operand);
    }

    if (field_str.find('-') != std::string::npos) {
      auto operands = string::split(field_str, "-");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
      if (operands[0] == "L") {
        if constexpr (std::is_same<day, FieldType>())
          return std::make_unique<LastNthDayInMonthField>(parse<days>(operands[1]));
      }
      return std::make_unique<RangeField<FieldType>>(parse<FieldType>(operands[0]), parse<FieldType>(operands[1]));
    }

    if (field_str.ends_with('L')) {
      if constexpr (!std::is_same<weekday, FieldType>())
        throw BadCronExpression("<X>L can only be used in the Day of week field");
      auto prefix = field_str.substr(0, field_str.size()-1);
      return std::make_unique<LastSpecificDayOfTheWeekOfTheMonth>(parse<weekday>(prefix));
    }

    if (field_str.find('/') != std::string::npos) {
      auto operands = string::split(field_str, "/");
      if (operands.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
      if (operands[0] == "*")
        operands[0] = "0";
      if (auto second_operand = fromChars<int>(operands[1]))
        return std::make_unique<IncrementField<FieldType>>(parse<FieldType>(operands[0]), *second_operand);
    }

    if (field_str.find(',') != std::string::npos) {
      auto operands_str = string::split(field_str, ",");
      std::vector<FieldType> operands;
      std::transform(operands_str.begin(), operands_str.end(), std::back_inserter(operands), parse<FieldType>);
      return std::make_unique<ListField<FieldType>>(std::move(operands));
    }

    if (field_str.ends_with('W')) {
      if constexpr (!std::is_same<day, FieldType>())
        throw BadCronExpression("W can only be used in the Day of month field");
      auto operands_str = string::split(field_str, "W");
      if (operands_str.size() != 2)
        throw BadCronExpression("Invalid field " + field_str);
      return std::make_unique<ClosestWeekdayToTheNthDayOfTheMonth>(parse<day>(operands_str[0]));
    }

    return std::make_unique<SingleValueField<FieldType>>(parse<FieldType>(field_str));
  } catch (const std::exception& e) {
    throw BadCronExpression("Couldn't parse cron field: " + field_str + " " + e.what());
  }
}
}  // namespace

Cron::Cron(const std::string& expression) {
  auto tokens = string::split(expression, " ");

  if (tokens.size() != 6 && tokens.size() != 7)
    throw BadCronExpression("malformed cron string (must be 6 or 7 fields): " + expression);

  second_ = parseCronField<seconds>(tokens[0]);
  minute_ = parseCronField<minutes>(tokens[1]);
  hour_ = parseCronField<hours>(tokens[2]);
  day_ = parseCronField<day>(tokens[3]);
  month_ = parseCronField<month>(tokens[4]);
  day_of_week_ = parseCronField<weekday>(tokens[5]);
  if (tokens.size() == 7)
    year_ = parseCronField<year>(tokens[6]);
}

std::optional<local_seconds> Cron::calculateNextTrigger(const local_seconds start) const {
  gsl_Expects(second_ && minute_ && hour_ && day_ && month_ && day_of_week_);
  auto next = timeutils::roundToNextSecond(start);
  while (next < local_days((year(2999)/1/1))) {
    if (year_ && !year_->matches(next)) {
      next = timeutils::roundToNextYear(next);
      continue;
    }
    if (!month_->matches(next)) {
      next = timeutils::roundToNextMonth(next);
      continue;
    }
    if (!day_->matches(next)) {
      next = timeutils::roundToNextDay(next);
      continue;
    }
    if (!day_of_week_->matches(next)) {
      next = timeutils::roundToNextDay(next);
      continue;
    }
    if (!hour_->matches(next)) {
      next = timeutils::roundToNextHour(next);
      continue;
    }
    if (!minute_->matches(next)) {
      next = timeutils::roundToNextMinute(next);
      continue;
    }
    if (!second_->matches(next)) {
      next = timeutils::roundToNextSecond(next);
      continue;
    }
    return next;
  }
  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils
