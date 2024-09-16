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
#pragma once

#include <cstring>
#include <ctime>

#include <array>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <optional>
#include <functional>
#include <algorithm>
#include <condition_variable>
#include <memory>

#include "StringUtils.h"

// libc++ doesn't define operator<=> on durations, and apparently the operator rewrite rules don't automagically make one
#if defined(_LIBCPP_VERSION)
#include <compare>
#include <concepts>
#endif

#include "date/date.h"

#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 170000
namespace org::apache::nifi::minifi::detail {
template<typename T>
concept has_spaceship_operator = requires(T t, T u) { t <=> u; };  // NOLINT(readability/braces)
}  // namespace org::apache::nifi::minifi::detail

static_assert(!org::apache::nifi::minifi::detail::has_spaceship_operator<std::chrono::duration<double>>,
  "Current libc++ version supports spaceship operator for durations, remove this workaround!");

template<typename Rep1, typename Period1, typename Rep2, typename Period2>
std::strong_ordering operator<=>(std::chrono::duration<Rep1, Period1> lhs, std::chrono::duration<Rep2, Period2> rhs) {
  if (lhs < rhs) {
    return std::strong_ordering::less;
  } else if (lhs == rhs) {
    return std::strong_ordering::equal;
  } else {
    return std::strong_ordering::greater;
  }
}
#endif

namespace org::apache::nifi::minifi::utils::timeutils {

/**
 * Gets the current time in nanoseconds
 * @returns nanoseconds since epoch
 */
inline uint64_t getTimeNano() {
  // The precision is platform dependent (1 ns on libstdc++, 0.1 us on msvc and 1 us on libc++)
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/**
 * Mockable clock classes
 */
class Clock {
 public:
  virtual ~Clock() = default;
  virtual std::chrono::milliseconds timeSinceEpoch() const = 0;
  virtual bool wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lck, std::chrono::milliseconds time, const std::function<bool()>& pred) {
    return cv.wait_for(lck, time - timeSinceEpoch(), pred);
  }
};

class SystemClock : public Clock {
 public:
  std::chrono::milliseconds timeSinceEpoch() const override {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
  }
};

class SteadyClock : public Clock {
 public:
  std::chrono::milliseconds timeSinceEpoch() const override {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  }

  virtual std::chrono::time_point<std::chrono::steady_clock> now() const {
    return std::chrono::steady_clock::now();
  }
};

std::shared_ptr<SteadyClock> getClock();

// test-only utility to specify what clock to use
void setClock(std::shared_ptr<SteadyClock> clock);

inline std::string getTimeStr(std::chrono::system_clock::time_point tp) {
  std::ostringstream stream;
  date::to_stream(stream, TIME_FORMAT, std::chrono::floor<std::chrono::milliseconds>(tp));
  return stream.str();
}

inline std::optional<std::chrono::sys_seconds> parseDateTimeStr(const std::string& str) {
  std::istringstream stream(str);
  std::chrono::sys_seconds tp;
  date::from_stream(stream, "%Y-%m-%dT%H:%M:%SZ", tp);
  if (stream.fail() || (stream.peek() && !stream.eof()))
    return std::nullopt;
  return tp;
}

std::optional<std::chrono::system_clock::time_point> parseRfc3339(const std::string& str);

inline std::string getDateTimeStr(std::chrono::sys_seconds tp) {
  return date::format("%Y-%m-%dT%H:%M:%SZ", tp);
}

inline std::string getRFC2616Format(std::chrono::sys_seconds tp) {
  return date::format("%a, %d %b %Y %H:%M:%S %Z", tp);
}

inline date::sys_seconds to_sys_time(const std::tm& t) {
  using date::year;
  using date::month;
  using date::day;
  using std::chrono::hours;
  using std::chrono::minutes;
  using std::chrono::seconds;
  return date::sys_days{year{t.tm_year + 1900}/(t.tm_mon+1)/t.tm_mday} + hours{t.tm_hour} + minutes{t.tm_min} + seconds{t.tm_sec};
}

inline date::local_seconds to_local_time(const std::tm& t) {
  using date::year;
  using date::month;
  using date::day;
  using std::chrono::hours;
  using std::chrono::minutes;
  using std::chrono::seconds;
  return date::local_days{year{t.tm_year + 1900}/(t.tm_mon+1)/t.tm_mday} + hours{t.tm_hour} + minutes{t.tm_min} + seconds{t.tm_sec};
}

namespace details {

template<class Duration>
bool unit_matches(const std::string&) {
  return false;
}

template<>
inline bool unit_matches<std::chrono::nanoseconds>(const std::string& unit) {
  return unit == "ns" || unit == "nano" || unit == "nanos" || unit == "nanoseconds" || unit == "nanosecond";
}

template<>
inline bool unit_matches<std::chrono::microseconds>(const std::string& unit) {
  return unit == "us" || unit == "micro" || unit == "micros" || unit == "microseconds" || unit == "microsecond";
}

template<>
inline bool unit_matches<std::chrono::milliseconds>(const std::string& unit) {
  return unit == "msec" || unit == "ms" || unit == "millisecond" || unit == "milliseconds" || unit == "msecs" || unit == "millis" || unit == "milli";
}

template<>
inline bool unit_matches<std::chrono::seconds>(const std::string& unit) {
  return unit == "sec" || unit == "s" || unit == "second" || unit == "seconds" || unit == "secs";
}

template<>
inline bool unit_matches<std::chrono::minutes>(const std::string& unit) {
  return unit == "min" || unit == "m" || unit == "mins" || unit == "minute" || unit == "minutes";
}

template<>
inline bool unit_matches<std::chrono::hours>(const std::string& unit) {
  return unit == "h" || unit == "hr" || unit == "hour" || unit == "hrs" || unit == "hours";
}

template<>
inline bool unit_matches<std::chrono::days>(const std::string& unit) {
  return unit == "d" || unit == "day" || unit == "days";
}

template<>
inline bool unit_matches<std::chrono::weeks>(const std::string& unit) {
  return unit == "w" || unit == "wk" || unit == "wks" || unit == "week" || unit == "weeks";
}

template<>
inline bool unit_matches<std::chrono::months>(const std::string& unit) {
  return unit == "month" || unit == "months";
}

template<>
inline bool unit_matches<std::chrono::years>(const std::string& unit) {
  return unit == "y" || unit == "year" || unit == "years";
}

template<class TargetDuration, class SourceDuration>
std::optional<TargetDuration> cast_if_unit_matches(const std::string& unit, const int64_t value) {
  if (unit_matches<SourceDuration>(unit)) {
    return std::chrono::duration_cast<TargetDuration>(SourceDuration(value));
  } else {
    return std::nullopt;
  }
}

template<class TargetDuration, typename... T>
std::optional<TargetDuration> cast_to_matching_unit(std::string& unit, const int64_t value) {
  std::optional<TargetDuration> result;
  ((result = cast_if_unit_matches<TargetDuration, T>(unit, value)) || ...);
  return result;
}
}  // namespace details

template<class TargetDuration>
std::optional<TargetDuration> StringToDuration(const std::string& input) {
  std::string unit;
  int64_t value;
  if (!string::splitToValueAndUnit(input, value, unit))
    return std::nullopt;


  unit = utils::string::toLower(unit);

  return details::cast_to_matching_unit<TargetDuration,
    std::chrono::nanoseconds,
    std::chrono::microseconds,
    std::chrono::milliseconds,
    std::chrono::seconds,
    std::chrono::minutes,
    std::chrono::hours,
    std::chrono::days,
    std::chrono::weeks,
    std::chrono::months,
    std::chrono::years>(unit, value);
}

inline date::local_seconds roundToNextYear(date::local_seconds tp) {
  date::year_month_day date(std::chrono::floor<std::chrono::days>(tp));
  auto start_of_year = date.year()/1/1;
  return date::local_days(start_of_year + std::chrono::years(1));
}

inline date::local_seconds roundToNextMonth(date::local_seconds tp) {
  date::year_month_day date(std::chrono::floor<std::chrono::days>(tp));
  auto start_of_month = date.year()/date.month()/1;
  return date::local_days(start_of_month + std::chrono::months(1));
}

inline date::local_seconds roundToNextDay(date::local_seconds tp) {
  return std::chrono::floor<std::chrono::days>(tp) + std::chrono::days(1);
}

inline date::local_seconds roundToNextHour(date::local_seconds tp) {
  return std::chrono::floor<std::chrono::hours>(tp) + std::chrono::hours(1);
}

inline date::local_seconds roundToNextMinute(date::local_seconds tp) {
  return std::chrono::floor<std::chrono::minutes>(tp) + std::chrono::minutes(1);
}

inline date::local_seconds roundToNextSecond(date::local_seconds tp) {
  return std::chrono::floor<std::chrono::seconds>(tp) + std::chrono::seconds(1);
}

#ifdef WIN32
// The tzdata location is set as a global variable in date-tz library
// We need to set it from from libminifi to effect calls made from libminifi (on Windows)
void dateSetInstall(const std::string& install);
#endif

}  // namespace org::apache::nifi::minifi::utils::timeutils
