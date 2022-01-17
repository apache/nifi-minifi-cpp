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
#ifndef LIBMINIFI_INCLUDE_UTILS_TIMEUTIL_H_
#define LIBMINIFI_INCLUDE_UTILS_TIMEUTIL_H_

#include <cstring>
#include <ctime>

#include <array>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace timeutils {

/**
 * Gets the current time in milliseconds
 * @returns milliseconds since epoch
 */
inline uint64_t getTimeMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/**
 * Gets the current time in nanoseconds
 * @returns nanoseconds since epoch
 */
inline uint64_t getTimeNano() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/**
 * Mockable clock classes
 */
class Clock {
 public:
  virtual ~Clock() = default;
  virtual std::chrono::milliseconds timeSinceEpoch() const = 0;
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
};

/**
 * Returns a string based on TIME_FORMAT, converting
 * the parameter to a string
 * @param msec milliseconds since epoch
 * @returns string representing the time
 */
inline std::string getTimeStr(uint64_t msec, bool enforce_locale = false) {
  char date[120];
  time_t second = (time_t) (msec / 1000);
  msec = msec % 1000;
  strftime(date, sizeof(date) / sizeof(*date), TIME_FORMAT, (enforce_locale == true ? gmtime(&second) : localtime(&second)));

  std::string ret = date;
  date[0] = '\0';
  sprintf(date, ".%03llu", (unsigned long long) msec); // NOLINT

  ret += date;
  return ret;
}

inline time_t mkgmtime(struct tm* date_time) {
#ifdef WIN32
  return _mkgmtime(date_time);
#else
  static const int month_lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static const int month_lengths_leap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static const auto is_leap_year = [](int year) -> bool {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  };
  static const auto num_leap_days = [](int year) -> int {
    return (year - 1968) / 4 - (year - 1900) / 100 + (year - 1600) / 400;
  };

  int year = date_time->tm_year + 1900;
  time_t result = year - 1970;
  result *= 365;
  result += num_leap_days(year - 1);

  for (int i = 0; i < 12 && i < date_time->tm_mon; ++i) {
    result += is_leap_year(year) ? month_lengths_leap[i] : month_lengths[i];
  }

  result += date_time->tm_mday - 1;
  result *= 24;

  result += date_time->tm_hour;
  result *= 60;

  result += date_time->tm_min;
  result *= 60;

  result += date_time->tm_sec;
  return result;
#endif
}

/**
 * Parse a datetime in yyyy-MM-dd'T'HH:mm:ssZ format
 * @param str the datetime string
 * @returns Unix timestamp
 */
inline int64_t parseDateTimeStr(const std::string& str) {
  /**
   * There is no strptime on Windows. As long as we have to parse a single date format this is not so bad,
   * but if multiple formats will have to be supported in the future, it might be worth it to include
   * an strptime implementation from some BSD on Windows.
   */
  uint32_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  int read = 0;
  if (sscanf(str.c_str(), "%4u-%2hhu-%2hhuT%2hhu:%2hhu:%2hhuZ%n", &year, &month, &day, &hours, &minutes, &seconds, &read) != 6) {
    return -1;
  }
  // while it is unlikely that read will be < 0, the conditional adds little cost for a little defensiveness.
  if (read < 0 || static_cast<size_t>(read) != str.size()) {
    return -1;
  }

  if (year < 1970U ||
      month > 12U ||
      day > 31U ||
      hours > 23U ||
      minutes > 59U ||
      seconds > 60U) {
    return -1;
  }

  struct tm timeinfo{};
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = hours;
  timeinfo.tm_min = minutes;
  timeinfo.tm_sec = seconds;
  timeinfo.tm_isdst = 0;

  return static_cast<int64_t>(mkgmtime(&timeinfo));
}

inline bool getDateTimeStr(int64_t unix_timestamp, std::string& date_time_str) {
  if (unix_timestamp > (std::numeric_limits<time_t>::max)() || unix_timestamp < (std::numeric_limits<time_t>::lowest)()) {
    return false;
  }
  time_t time = static_cast<time_t>(unix_timestamp);
  struct tm* gmt = gmtime(&time); // NOLINT
  if (gmt == nullptr) {
    return false;
  }
  std::array<char, 64U> buf;
  if (strftime(buf.data(), buf.size(), "%Y-%m-%dT%H:%M:%SZ", gmt) == 0U) {
    return false;
  }

  date_time_str = buf.data();
  return true;
}

} /* namespace timeutils */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

// for backwards compatibility, to be removed after 0.8
using org::apache::nifi::minifi::utils::timeutils::getTimeMillis;
using org::apache::nifi::minifi::utils::timeutils::getTimeNano;
using org::apache::nifi::minifi::utils::timeutils::getTimeStr;
using org::apache::nifi::minifi::utils::timeutils::parseDateTimeStr;
using org::apache::nifi::minifi::utils::timeutils::getDateTimeStr;

#endif  // LIBMINIFI_INCLUDE_UTILS_TIMEUTIL_H_
