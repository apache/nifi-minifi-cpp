/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenseas/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIBMINIFI_INCLUDE_UTILS_VALUEPARSER_H_
#define LIBMINIFI_INCLUDE_UTILS_VALUEPARSER_H_

#include <exception>
#include <string>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <type_traits>
#include <limits>
#include <algorithm>

#include "PropertyErrors.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace internal {

class ValueParser {
 public:
  explicit ValueParser(const std::string& str, std::size_t offset = 0) : str(str), offset(offset) {}

  ValueParser& parse(int& out) {  // NOLINT
    long result;  // NOLINT
    auto len = safeCallConverter(std::strtol, result);
    if (len == 0) {
      throw ParseException("Couldn't parse int");
    }
    if (result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max()) {
      throw ParseException("Cannot convert long to int");
    }
    offset += len;
    out = result;
    return *this;
  }

  ValueParser& parse(int64_t& out) {
    long long result;  // NOLINT
    auto len = safeCallConverter(std::strtoll, result);
    if (len == 0) {
      throw ParseException("Couldn't parse long long");
    }
    if (result < std::numeric_limits<int64_t>::min() || result > std::numeric_limits<int64_t>::max()) {
      throw ParseException("Cannot convert long long to int64_t");
    }
    offset += len;
    out = result;
    return *this;
  }

  ValueParser& parse(uint32_t & out) {
    skipWhitespace();
    if (offset < str.length() && str[offset] == '-') {
      throw ParseException("Not an unsigned long");
    }
    unsigned long result;  // NOLINT
    auto len = safeCallConverter(std::strtoul, result);
    if (len == 0) {
      throw ParseException("Couldn't parse uint32_t");
    }
    if (result > std::numeric_limits<uint32_t>::max()) {
      throw ParseException("Cannot convert unsigned long to uint32_t");
    }
    offset += len;
    out = result;
    return *this;
  }

  ValueParser& parse(uint64_t& out) {
    skipWhitespace();
    if (offset < str.length() && str[offset] == '-') {
      throw ParseException("Not an unsigned long");
    }
    unsigned long long result;  // NOLINT
    auto len = safeCallConverter(std::strtoull, result);
    if (len == 0) {
      throw ParseException("Couldn't parse unsigned long long");
    }
    if (result > std::numeric_limits<uint64_t>::max()) {
      throw ParseException("Cannot convert unsigned long long to uint64_t");
    }
    offset += len;
    out = result;
    return *this;
  }

  ValueParser& parse(bool& out) {
    skipWhitespace();
    if (std::strncmp(str.c_str() + offset, "false", std::strlen("false")) == 0) {
      offset += std::strlen("false");
      out = false;
    } else if (std::strncmp(str.c_str() + offset, "true", std::strlen("true")) == 0) {
      offset += std::strlen("true");
      out = true;
    } else {
      throw ParseException("Couldn't parse bool");
    }
    return *this;
  }

  void parseEnd() {
    skipWhitespace();
    if (offset < str.length()) {
      throw ParseException("Expected to parse till the end");
    }
  }

 private:
  /**
   *
   * @tparam T
   * @param converter
   * @param out
   * @return the number of characters used during conversion, 0 for error
   */
  template<typename T>
  std::size_t safeCallConverter(T (*converter)(const char* begin, char** end, int base), T& out) {
    const char* const begin = str.c_str() + offset;
    char* end;
    errno = 0;
    T result = converter(begin, &end, 10);
    if (end == begin || errno == ERANGE) {
      return 0;
    }
    out = result;
    return end - begin;
  }

  void skipWhitespace() {
    while (offset < str.length() && std::isspace(str[offset])) {
      ++offset;
    }
  }

  const std::string& str;
  std::size_t offset;
};

template<typename Out>
bool StringToTime(const std::string& input, Out& output, core::TimeUnit& timeunit) {
  if (input.size() == 0) {
    return false;
  }

  const char* begin = input.c_str();
  char *end;
  errno = 0;
  auto ival = std::strtoll(begin, &end, 0);
  if (end == begin || errno == ERANGE) {
    return false;
  }

  if (end[0] == '\0') {
    return false;
  }

  while (*end == ' ') {
    // Skip the space
    end++;
  }

  std::string unit(end);
  std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

  if (unit == "ns" || unit == "nano" || unit == "nanos" || unit == "nanoseconds") {
    timeunit = core::TimeUnit::NANOSECOND;
    output = ival;
    return true;
  } else if (unit == "us" || unit == "micro" || unit == "micros" || unit == "microseconds" || unit == "microsecond") {
    timeunit = core::TimeUnit::MICROSECOND;
    output = ival;
    return true;
  } else if (unit == "msec" || unit == "ms" || unit == "millisecond" || unit == "milliseconds" || unit == "msecs" || unit == "millis" || unit == "milli") {
    timeunit = core::TimeUnit::MILLISECOND;
    output = ival;
    return true;
  } else if (unit == "sec" || unit == "s" || unit == "second" || unit == "seconds" || unit == "secs") {
    timeunit = core::TimeUnit::SECOND;
    output = ival;
    return true;
  } else if (unit == "min" || unit == "m" || unit == "mins" || unit == "minute" || unit == "minutes") {
    timeunit = core::TimeUnit::MINUTE;
    output = ival;
    return true;
  } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hrs" || unit == "hours") {
    timeunit = core::TimeUnit::HOUR;
    output = ival;
    return true;
  } else if (unit == "d" || unit == "day" || unit == "days") {
    timeunit = core::TimeUnit::DAY;
    output = ival;
    return true;
  } else {
    return false;
  }
}
} /* namespace internal */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_UTILS_VALUEPARSER_H_
