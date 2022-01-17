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

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>

#include "state/Value.h"
#include "utils/StringUtils.h"
#include "utils/ValueParser.h"
#include "utils/PropertyErrors.h"
#include "utils/Literals.h"
#include "utils/Export.h"
#include "utils/TimeUtil.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class TransformableValue {
 public:
  TransformableValue() = default;
};


/**
 * Purpose and Design: TimePeriodValue represents a time period that can be set via a numeric followed by
 * a time unit string. This Value is based on uint64, but has the support to return
 * the original string representation. Once set, both are immutable.
 */
class TimePeriodValue : public TransformableValue, public state::response::UInt64Value {
 public:
  MINIFIAPI static const std::type_index type_id;

  explicit TimePeriodValue(const std::string &timeString)
      : state::response::UInt64Value(0) {
    auto parsed_time = utils::timeutils::StringToDuration<std::chrono::milliseconds>(timeString);
    if (!parsed_time) {
      throw utils::internal::ParseException("Couldn't parse TimePeriodValue");
    }
    string_value = timeString;
    value = parsed_time->count();
  }

  explicit TimePeriodValue(uint64_t value)
      : state::response::UInt64Value(value) {
  }

  TimePeriodValue()
      : state::response::UInt64Value(0) {
  }

  std::chrono::milliseconds getMilliseconds() const {
    return std::chrono::milliseconds(getValue());
  }

  static std::optional<TimePeriodValue> fromString(const std::string& str) {
    try {
      return TimePeriodValue(str);
    } catch (const utils::internal::ValueException&) {
      return std::nullopt;
    }
  }
};

/**
 * Purpose and Design: DataSizeValue represents a file system size value that extends
 * Uint64Value. This means that a string is converted to uint64_t. The string is of the
 * format <numeric> <byte size>.
 */
class DataSizeValue : public TransformableValue, public state::response::UInt64Value {
  static std::shared_ptr<logging::Logger>& getLogger();

 public:
  MINIFIAPI static const std::type_index type_id;

  explicit DataSizeValue(const std::string &sizeString)
      : state::response::UInt64Value(0) {
    StringToInt<uint64_t>(sizeString, value);
    string_value = sizeString;
  }

  explicit DataSizeValue(uint64_t value)
      : state::response::UInt64Value(value) {
  }

  DataSizeValue()
      : state::response::UInt64Value(0) {
  }


  // Convert String to Integer
  template<typename T, typename std::enable_if<
      std::is_integral<T>::value>::type* = nullptr>
  static bool StringToInt(const std::string &input, T &output) {
    // TODO(adebreceni): this mapping is to preserve backwards compatibility,
    //  we should entertain the idea of moving to standardized units in
    //  the configuration (i.e. K = 1000, Ki = 1024)
    static std::map<std::string, int64_t> unit_map{
      {"B", 1},
      {"K", 1_KB}, {"M", 1_MB}, {"G", 1_GB}, {"T", 1_TB}, {"P", 1_PB},
      {"KB", 1_KiB}, {"MB", 1_MiB}, {"GB", 1_GiB}, {"TB", 1_TiB}, {"PB", 1_PiB},
    };

    int64_t value;
    std::string unit_str;
    try {
      unit_str = utils::StringUtils::trim(utils::internal::ValueParser(input).parse(value).rest());
    } catch (const utils::internal::ParseException&) {
      return false;
    }

    if (!unit_str.empty()) {
      std::transform(unit_str.begin(), unit_str.end(), unit_str.begin(), ::toupper);
      auto multiplierIt = unit_map.find(unit_str);
      if (multiplierIt == unit_map.end()) {
        getLogger()->log_warn("Unrecognized data unit: '%s', in the future this will constitute as an error", unit_str);
        // backwards compatibility
        // return false;
      } else {
        value *= multiplierIt->second;
      }
    }

    output = gsl::narrow<T>(value);
    return true;
  }
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_TYPEDVALUES_H_
