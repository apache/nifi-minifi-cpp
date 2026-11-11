// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <exception>
#include <string>
#include <cstring>
#include <vector>
#include <cstdlib>
#include <type_traits>
#include <limits>
#include <algorithm>

#include "minifi-cpp/utils/PropertyExceptions.h"

namespace org::apache::nifi::minifi::utils {
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

  ValueParser& parse(double& out) {
    double result(0);
    auto len = safeCallConverter(std::strtod, result);
    if (len == 0) {
      throw ParseException("Couldn't parse double");
    }
    offset += len;
    out = result;
    return *this;
  }

  void parseEnd() {
    skipWhitespace();
    if (offset < str.length()) {
      throw ParseException("Expected to parse till the end");
    }
  }

  std::string rest() const noexcept {
    return str.substr(offset);
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

  /**
 *
 * @tparam T
 * @param converter
 * @param out
 * @return the number of characters used during conversion, 0 for error
 */
  template<typename T>
  std::size_t safeCallConverter(T (*converter)(const char* begin, char** end), T& out) {
    const char* const begin = str.c_str() + offset;
    char* end;
    errno = 0;
    T result = converter(begin, &end);
    if (end == begin || errno == ERANGE) {
      return 0;
    }
    out = result;
    return end - begin;
  }

  void skipWhitespace() {
    while (offset < str.length() && std::isspace(static_cast<unsigned char>(str[offset]))) {
      ++offset;
    }
  }

  const std::string& str;
  std::size_t offset;
};
}  // namespace internal

template<typename T>
std::optional<T> toNumber(const std::string& input) {
  try {
    T output;
    internal::ValueParser(input).parse(output).parseEnd();
    return output;
  } catch(const internal::ParseException&) {
    return std::nullopt;
  }
}

}  // namespace org::apache::nifi::minifi::utils
