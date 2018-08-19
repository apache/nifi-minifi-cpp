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
#ifndef LIBMINIFI_INCLUDE_CORE_TYPEDVALUES_H_
#define LIBMINIFI_INCLUDE_CORE_TYPEDVALUES_H_

#include "state/Value.h"
#include <typeindex>
#include "utils/StringUtils.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class TransformableValue {
 public:
  TransformableValue() {
  }
};


/**
 * Purpose and Design: TimePeriodValue represents a time period that can be set via a numeric followed by
 * a time unit string. This Value is based on uint64, but has the support to return
 * the original string representation. Once set, both are immutable.
 */
class TimePeriodValue : public TransformableValue, public state::response::UInt64Value {
 public:
  static const std::type_index type_id;

  explicit TimePeriodValue(const std::string &timeString)
      : state::response::UInt64Value(0) {
    TimeUnit units;
    StringToTime(timeString, value, units);
    string_value = timeString;
    ConvertTimeUnitToMS<uint64_t>(value, units, value);
  }

  explicit TimePeriodValue(uint64_t value)
      : state::response::UInt64Value(value) {
  }

  // Convert TimeUnit to MilliSecond
  template<typename T>
  static bool ConvertTimeUnitToMS(T input, TimeUnit unit, T &out) {
    if (unit == MILLISECOND) {
      out = input;
      return true;
    } else if (unit == SECOND) {
      out = input * 1000;
      return true;
    } else if (unit == MINUTE) {
      out = input * 60 * 1000;
      return true;
    } else if (unit == HOUR) {
      out = input * 60 * 60 * 1000;
      return true;
    } else if (unit == DAY) {
      out = 24 * 60 * 60 * 1000;
      return true;
    } else if (unit == NANOSECOND) {
      out = input / 1000 / 1000;
      return true;
    } else {
      return false;
    }
  }

  static bool StringToTime(std::string input, uint64_t &output, TimeUnit &timeunit) {
    if (input.size() == 0) {
      return false;
    }

    const char *cvalue = input.c_str();
    char *pEnd;
    auto ival = std::strtoll(cvalue, &pEnd, 0);

    if (pEnd[0] == '\0') {
      return false;
    }

    while (*pEnd == ' ') {
      // Skip the space
      pEnd++;
    }

    std::string unit(pEnd);
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    if (unit == "sec" || unit == "s" || unit == "second" || unit == "seconds" || unit == "secs") {
      timeunit = SECOND;
      output = ival;
      return true;
    } else if (unit == "msec" || unit == "ms" || unit == "millisecond" || unit == "milliseconds" || unit == "msecs") {
      timeunit = MILLISECOND;
      output = ival;
      return true;
    } else if (unit == "min" || unit == "m" || unit == "mins" || unit == "minute" || unit == "minutes") {
      timeunit = MINUTE;
      output = ival;
      return true;
    } else if (unit == "ns" || unit == "nano" || unit == "nanos" || unit == "nanoseconds") {
      timeunit = NANOSECOND;
      output = ival;
      return true;
    } else if (unit == "ms" || unit == "milli" || unit == "millis" || unit == "milliseconds") {
      timeunit = MILLISECOND;
      output = ival;
      return true;
    } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hrs" || unit == "hours") {
      timeunit = HOUR;
      output = ival;
      return true;
    } else if (unit == "d" || unit == "day" || unit == "days") {
      timeunit = DAY;
      output = ival;
      return true;
    } else
      return false;
  }
};

/**
 * Purpose and Design: DataSizeValue represents a file system size value that extends
 * Uint64Value. This means that a string is converted to uint64_t. The string is of the
 * format <numeric> <byte size>.
 */
class DataSizeValue : public TransformableValue, public state::response::UInt64Value {
 public:
  static const std::type_index type_id;

  explicit DataSizeValue(const std::string &sizeString)
      : state::response::UInt64Value(0) {
    StringToInt<uint64_t>(sizeString, value);
    string_value = sizeString;
  }

  explicit DataSizeValue(uint64_t value)
      : state::response::UInt64Value(value) {
  }

  // Convert String to Integer
  template<typename T>
  static bool StringToInt(const std::string &input, T &output) {
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
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_TYPEDVALUES_H_ */
