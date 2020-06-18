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

#include "PropertyErrors.h"
#include "GeneralUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace internal {

class ValueParser {
 private:
  template<typename From, typename To, typename = void>
  struct is_non_narrowing_convertible : std::false_type {
    static_assert(std::is_integral<From>::value && std::is_integral<To>::value, "Checks only integral values");
  };

  template<typename From, typename To>
  struct is_non_narrowing_convertible<From, To, void_t<decltype(To{std::declval<From>()})>> : std::true_type {
    static_assert(std::is_integral<From>::value && std::is_integral<To>::value, "Checks only integral values");
  };

 public:
  explicit ValueParser(const std::string& str, std::size_t offset = 0) : str(str), offset(offset) {}

  template<typename Out>
  ValueParser& parseInt(Out& out) {
    static_assert(is_non_narrowing_convertible<int, Out>::value, "Expected lossless conversion from int");
    long result;  // NOLINT
    auto len = safeCallConverter(std::strtol, result);
    if (len == 0) {
      throw ParseException("Couldn't parse int");
    }
    if (result < (std::numeric_limits<int>::min)() || result > (std::numeric_limits<int>::max)()) {
      throw ParseException("Cannot convert long to int");
    }
    offset += len;
    out = {static_cast<int>(result)};
    return *this;
  }

  template<typename Out>
  ValueParser& parseLong(Out& out) {
    static_assert(is_non_narrowing_convertible<long, Out>::value, "Expected lossless conversion from long");  // NOLINT
    long result;  // NOLINT
    auto len = safeCallConverter(std::strtol, result);
    if (len == 0) {
      throw ParseException("Couldn't parse long");
    }
    offset += len;
    out = {result};
    return *this;
  }

  template<typename Out>
  ValueParser& parseLongLong(Out& out) {
    static_assert(is_non_narrowing_convertible<long long, Out>::value, "Expected lossless conversion from long long");  // NOLINT
    long long result;  // NOLINT
    auto len = safeCallConverter(std::strtoll, result);
    if (len == 0) {
      throw ParseException("Couldn't parse long long");
    }
    offset += len;
    out = {result};
    return *this;
  }

  template<typename Out>
  ValueParser& parseUInt32(Out& out) {
    static_assert(is_non_narrowing_convertible<uint32_t, Out>::value, "Expected lossless conversion from uint32_t");
    skipWhitespace();
    if (offset < str.length() && str[offset] == '-') {
      throw ParseException("Not an unsigned long");
    }
    unsigned long result;  // NOLINT
    auto len = safeCallConverter(std::strtoul, result);
    if (len == 0) {
      throw ParseException("Couldn't parse uint32_t");
    }
    if (result > (std::numeric_limits<uint32_t>::max)()) {
      throw ParseException("Cannot convert unsigned long to uint32_t");
    }
    offset += len;
    out = {static_cast<uint32_t>(result)};
    return *this;
  }

  template<typename Out>
  ValueParser& parseUnsignedLongLong(Out& out) {
    static_assert(is_non_narrowing_convertible<unsigned long long, Out>::value, "Expected lossless conversion from unsigned long long");  // NOLINT
    skipWhitespace();
    if (offset < str.length() && str[offset] == '-') {
      throw ParseException("Not an unsigned long");
    }
    unsigned long long result;  // NOLINT
    auto len = safeCallConverter(std::strtoull, result);
    if (len == 0) {
      throw ParseException("Couldn't parse unsigned long long");
    }
    offset += len;
    out = {result};
    return *this;
  }

  template<typename Out>
  ValueParser& parseBool(Out& out) {
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

} /* namespace internal */
} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_UTILS_VALUEPARSER_H_
