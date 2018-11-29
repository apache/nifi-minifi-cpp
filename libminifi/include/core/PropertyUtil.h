/**
 *
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

#ifndef NIFI_MINIFI_CPP_PROPERTYUTIL_H
#define NIFI_MINIFI_CPP_PROPERTYUTIL_H

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

template<typename T>
struct assert_false : std::false_type
{ };

static bool StringToBool(std::string input, bool &output) {
  std::transform(input.begin(), input.end(), input.begin(), ::tolower);
  if(input == "true") {
    output = true;
    return true;
  }
  if(input == "false") {
    output = false;
    return true;
  }
  return false;
}

// Convert String to signed Integer
template<typename T,  typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, T>::type* = nullptr>
static bool StringToInt(std::string input, T &output) {
  if (input.size() == 0) {
    return false;
  }

  const char *cvalue = input.c_str();
  char *pEnd;
  auto ival = std::strtoll(cvalue, &pEnd, 0);

  if (pEnd[0] == '\0') {
    output = ival;
    return true;
  }

  while (*pEnd == ' ') {
    // Skip the space
    pEnd++;
  }

  char end0 = toupper(pEnd[0]);
  if (end0 == 'B') {
    output = ival;
    return true;
  } else if ((end0 == 'K') || (end0 == 'M') || (end0 == 'G') || (end0 == 'T') || (end0 == 'P')) {
    if (pEnd[1] == '\0') {
      unsigned long int multiplier = 1000;

      if ((end0 != 'K')) {
        multiplier *= 1000;
        if (end0 != 'M') {
          multiplier *= 1000;
          if (end0 != 'G') {
            multiplier *= 1000;
            if (end0 != 'T') {
              multiplier *= 1000;
            }
          }
        }
      }

      output = ival * multiplier;
      return true;

    } else if ((pEnd[1] == 'b' || pEnd[1] == 'B') && (pEnd[2] == '\0')) {

      unsigned long int multiplier = 1024;

      if ((end0 != 'K')) {
        multiplier *= 1024;
        if (end0 != 'M') {
          multiplier *= 1024;
          if (end0 != 'G') {
            multiplier *= 1024;
            if (end0 != 'T') {
              multiplier *= 1024;
            }
          }
        }
      }
      output = ival * multiplier;
      return true;
    }
  }

  return false;
}

//Unsigned int conversion
template<typename T,  typename std::enable_if<std::is_integral<T>::value && !std::is_signed<T>::value, T>::type* = nullptr>
static bool StringToInt(std::string input, T &output) {
  typedef typename std::make_signed<T>::type signed_type;
  signed_type value;
  if(StringToInt<signed_type>(input, value) && (value >= 0)) {
    output = value;
    return true;
  }
  return false;
}

//Can't convert non-integral without specialization
template <typename T, typename std::enable_if<!std::is_integral<T>::value, T>::type* = nullptr>
static inline bool StringToType(std::string str, T& out) {
  static_assert( assert_false<T>::value , "No conversion is implemented for your type!");
};

//Convert integral
template <typename T, typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr>
static inline bool StringToType(std::string str, T& out) {
  return StringToInt(str, out);
};

static inline bool StringToType(std::string str, bool& out) {
  return StringToBool(str, out);
};

static inline bool StringToType(std::string str, std::string& out) {
  out = str;
  return true;
}

static inline bool StringToType(std::string str, double& out) {
  try {
    out = std::stod(str);
    return true;
  } catch (...) {
    return false;
  }
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_PROPERTYUTIL_H
