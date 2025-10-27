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

#include <iomanip>
#include <limits>
#include <string>
#include <sstream>
#include <utility>
#include <variant>
#include "utils/GeneralUtils.h"
#include "utils/StringUtils.h"

#pragma once

namespace org::apache::nifi::minifi::expression {

/**
 * Represents an expression value, which can be one of multiple types or NULL (represented as variant with monostate as null)
 */
class Value {
 public:
  /// Construct a default (NULL) value
  Value() = default;

  /// Construct with the specified value
  explicit Value(std::string val) :value_{std::move(val)} {}
  explicit Value(bool val) :value_{val} {}
  explicit Value(uint64_t val) :value_{val} {}
  explicit Value(int64_t val) :value_{val} {}
  explicit Value(long double val) :value_{val} {}

  [[nodiscard]] bool isNull() const { return holds_alternative<std::monostate>(value_); }

  [[nodiscard]] bool isDecimal() const {
    return std::visit(utils::overloaded{
      [](long double) { return true; },
      [](const std::string& string_val) {
        return string_val.find('.') != string_val.npos ||
            string_val.find('e') != string_val.npos ||
            string_val.find('E') != string_val.npos;
      },
      [](const auto&) { return false; }
    }, value_);
  }

  void setSignedLong(int64_t val) { value_ = val; }
  void setUnsignedLong(uint64_t val) { value_ = val; }
  void setLongDouble(long double val) { value_ = val; }
  void setBoolean(bool val) { value_ = val; }
  void setString(std::string val) { value_ = std::move(val); }

  [[nodiscard]] std::string asString() const {
    return std::visit<std::string>(utils::overloaded{
      [](const std::string& str) { return str; },
      [](bool b) -> std::string { return b ? "true" : "false"; },
      [](int64_t i) { return std::to_string(i); },
      [](uint64_t i) { return std::to_string(i); },
      [](long double d) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(std::numeric_limits<double>::digits10)
           << d;
        auto result = ss.str();
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);

        if (result.find('.') == result.length() - 1) {
          result.erase(result.length() - 1, std::string::npos);
        }

        return result;
      },
      [](std::monostate) { return std::string{}; }
    }, value_);
  }

  [[nodiscard]] uint64_t asUnsignedLong() const {
    return std::visit<uint64_t>(utils::overloaded{
      [](uint64_t i) { return i; },
      [](int64_t i) { return gsl::narrow_cast<uint64_t>(i); },
      [](long double d) { return static_cast<uint64_t>(d); },
      [](const std::string& str) -> uint64_t {
        const auto stoull_ = [](const std::string& s) { return std::stoull(s); };
        return strParse<uint64_t>(stoull_, 0, "Value::asUnsignedLong", str);
      },
      [](auto) { return uint64_t{0}; }
    }, value_);
  }

  [[nodiscard]] int64_t asSignedLong() const {
    return std::visit<int64_t>(utils::overloaded{
      [](int64_t i) { return i; },
      [](uint64_t i) { return gsl::narrow_cast<int64_t>(i); },
      [](long double d) { return static_cast<int64_t>(d); },
      [](const std::string& str) -> int64_t {
        const auto stoll_ = [](const std::string& s) { return std::stoll(s); };
        return strParse<int64_t>(stoll_, 0, "Value::asSignedLong", str);
      },
      [](auto) { return int64_t{0}; }
    }, value_);
  }

  [[nodiscard]] long double asLongDouble() const {
    return std::visit<long double>(utils::overloaded{
      [](long double d) { return d; },
      [](int64_t i) { return static_cast<long double>(i); },
      [](uint64_t i) { return static_cast<long double>(i); },
      [](const std::string& str) -> long double {
        const auto stold_ = [](const std::string& s) { return std::stold(s); };
        return strParse<long double>(stold_, 0.0, "Value::asLongDouble", str);
      },
      [](auto) -> long double { return 0.0; }
    }, value_);
  }

  [[nodiscard]] bool asBoolean() const {
    return std::visit(utils::overloaded{
      [](bool b) { return b; },
      [](int64_t i) { return i != 0; },
      [](uint64_t i) { return i != 0; },
      [](long double d) { return d != 0.0; },
      [](const std::string& str) { return utils::string::toBool(str).value_or(false); },
      [](auto) { return false; }
    }, value_);
  }

 private:
  template<typename T>
  static T strParse(std::regular_invocable<std::string> auto const& conversion_function, T default_value, std::string_view context, const std::string& value) {
    if (value.empty()) return default_value;
    try {
      return std::invoke(conversion_function, value);
    } catch (const std::invalid_argument&) {
      throw std::invalid_argument{utils::string::join_pack(context, " failed to parse \"", value, "\": invalid argument")};
    } catch (const std::out_of_range&) {
      throw std::out_of_range{utils::string::join_pack(context, " failed to parse \"", value, "\": out of range")};
    }
  }

  std::variant<std::monostate, bool, uint64_t, int64_t, long double, std::string> value_;
};

}  // namespace org::apache::nifi::minifi::expression
