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

#ifndef NIFI_MINIFI_CPP_VALUEUTILS_H
#define NIFI_MINIFI_CPP_VALUEUTILS_H

#include <exception>
#include <string>
#include <cstring>
#include <vector>
#include <cstdlib>
#include "PropertyErrors.h"
#include <type_traits>
#include <limits>
#include "GeneralUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

class ValueParser {
 private:
  template<typename From, typename To, typename = void>
  struct is_non_narrowing_convertible: std::false_type {
    static_assert(std::is_integral<From>::value && std::is_integral<To>::value, "Checks only integral values");
  };

  template<typename From, typename To>
  struct is_non_narrowing_convertible<From, To, void_t<decltype(To{std::declval<From>()})>>: std::true_type{
    static_assert(std::is_integral<From>::value && std::is_integral<To>::value, "Checks only integral values");
  };
  
 public:
  ValueParser(const std::string& str, std::size_t offset = 0): str(str), offset(offset) {}

  template<typename Out>
  ValueParser& parseInt(Out& out) {
    static_assert(is_non_narrowing_convertible<int, Out>::value, "Expected lossless conversion from int");
    try {
      char *end;
      long result{std::strtol(str.c_str() + offset, &end, 10)};
      offset = end - str.c_str();
      if (result < (std::numeric_limits<int>::min)() || result > (std::numeric_limits<int>::max)()) {
        throw ParseException("Cannot convert long to int");
      }
      out = {static_cast<int>(result)};
      return *this;
    }catch(...){
      throw ParseException("Could not parse int");
    }
  }

  template<typename Out>
  ValueParser& parseLong(Out& out) {
    static_assert(is_non_narrowing_convertible<long, Out>::value, "Expected lossless conversion from long");
    try {
      char *end;
      long result{std::strtol(str.c_str() + offset, &end, 10)};
      offset = end - str.c_str();
      out = {result};
      return *this;
    }catch(...){
      throw ParseException("Could not parse long");
    }
  }

  template<typename Out>
  ValueParser& parseLongLong(Out& out) {
    static_assert(is_non_narrowing_convertible<long long, Out>::value, "Expected lossless conversion from long long");
    try {
      char *end;
      long long result{std::strtoll(str.c_str() + offset, &end, 10)};
      offset = end - str.c_str();
      out = {result};
      return *this;
    }catch(...){
      throw ParseException("Could not parse long long");
    }
  }

  template<typename Out>
  ValueParser& parseUInt32(Out& out) {
    static_assert(is_non_narrowing_convertible<uint32_t, Out>::value, "Expected lossless conversion from uint32_t");
    try {
      parseSpace();
      if (offset < str.length() && str[offset] == '-') {
        throw ParseException("Not an unsigned long");
      }
      char *end;
      unsigned long result{std::strtoul(str.c_str() + offset, &end, 10)};
      offset = end - str.c_str();
      if (result > (std::numeric_limits<uint32_t>::max)()) {
        throw ParseException("Cannot convert unsigned long to uint32_t");
      }
      out = {static_cast<uint32_t>(result)};
      return *this;
    }catch(...){
      throw ParseException("Could not parse unsigned long");
    }
  }

  template<typename Out>
  ValueParser& parseUnsignedLongLong(Out& out) {
    static_assert(is_non_narrowing_convertible<unsigned long long, Out>::value, "Expected lossless conversion from unsigned long long");
    try {
      parseSpace();
      if (offset < str.length() && str[offset] == '-') {
        throw ParseException("Not an unsigned long");
      }
      char *end;
      unsigned long long result{std::strtoull(str.c_str() + offset, &end, 10)};
      offset = end - str.c_str();
      out = {result};
      return *this;
    }catch(...){
      throw ParseException("Could not parse unsigned long long");
    }
  }

  template<typename Out>
  ValueParser& parseBool(Out& out){
    parseSpace();
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

  void parseEnd(){
    parseSpace();
    if(offset < str.length()){
      throw ParseException("Expected to parse till the end");
    }
  }

 private:
  void parseSpace() {
    while (offset < str.length() && std::isspace(str[offset])) {
      ++offset;
    }
  }

  const std::string& str;
  std::size_t offset;

};


} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_VALUEUTILS_H
