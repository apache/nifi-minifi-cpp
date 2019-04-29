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
#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H__

#include <time.h>
#include <cstdio>
#include <string.h>
#include <iomanip>
#include <sstream>
#include <chrono>

#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

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
  sprintf(date, ".%03llu", (unsigned long long) msec);

  ret += date;
  return ret;
}

/**
 * Parse a datetime in yyyy-MM-dd'T'HH:mm:ssZ format
 * @param str the datetime string
 * @returns Unix timestamp
 */
inline int64_t pareDateTimeStr(const std::string &str) {
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
  if (read != str.size()) {
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

  /* Get local timezone offset */
  time_t utc = time(nullptr);
  struct tm now_tm = *gmtime(&utc);
  now_tm.tm_isdst = 0;
  time_t local = mktime(&now_tm);
  if (local == -1) {
    return -1;
  }
  int64_t timezone_offset = utc - local;

  /* Convert parsed date */
  time_t time = mktime(&timeinfo);
  if (time == -1) {
    return -1;
  }
  return time + timezone_offset;
}

#endif
