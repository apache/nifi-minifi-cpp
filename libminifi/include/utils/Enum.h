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

#pragma once

#include <cstring>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "ArrayUtils.h"
#include "StringUtils.h"
#include "Macro.h"

namespace org::apache::nifi::minifi::utils {

#define INCLUDE_BASE_FIELD(x) \
  x = Base::x

// [[maybe_unused]] on public members to avoid warnings when used inside an anonymous namespace
#define SMART_ENUM_BODY(Clazz, ...) \
    constexpr Clazz(Type value = static_cast<Type>(-1)) : value_{value} {} \
    explicit Clazz(std::string_view str) : value_{parse(str).value_} {} \
    explicit Clazz(const std::string& str) : Clazz(std::string_view{str}) {} \
    explicit Clazz(std::nullptr_t) = delete; \
   private: \
    Type value_; \
   public: \
    [[maybe_unused]] constexpr Type value() const { \
      return value_; \
    } \
    struct detail : public Base::detail {}; \
    static constexpr int length = Base::length + COUNT(__VA_ARGS__); \
    static constexpr auto values = org::apache::nifi::minifi::utils::array_cat(Base::values, \
        std::array<std::string_view, COUNT(__VA_ARGS__)>{FOR_EACH(SECOND, COMMA, (__VA_ARGS__))}); \
    [[maybe_unused]] const char* toString() const { \
      int index = static_cast<int>(value_); \
      return values.at(index).data(); \
    } \
    [[maybe_unused]] friend const char* toString(Type a) { \
      return Clazz{a}.toString(); \
    } \
    [[maybe_unused]] const char* toStringOr(const char* fallback) const { \
      if (*this) { \
        return toString(); \
      } \
      return fallback; \
    } \
    [[maybe_unused]] friend constexpr bool operator==(Clazz lhs, Clazz rhs) { \
      return lhs.value_ == rhs.value_; \
    } \
    [[maybe_unused]] friend constexpr bool operator!=(Clazz lhs, Clazz rhs) { \
      return lhs.value_ != rhs.value_; \
    } \
    [[maybe_unused]] friend constexpr bool operator<(Clazz lhs, Clazz rhs) { \
      return lhs.value_ < rhs.value_;\
    } \
    [[maybe_unused]] explicit constexpr operator bool() const { \
      int idx = static_cast<int>(value_); \
      return 0 <= idx && idx < length; \
    } \
    [[maybe_unused]] static Clazz parse(std::string_view str, const ::std::optional<Clazz>& fallback = {}, bool caseSensitive = true) { \
      for (int idx = 0; idx < length; ++idx) { \
        if (::org::apache::nifi::minifi::utils::StringUtils::equals(str, values.at(idx), caseSensitive)) \
          return static_cast<Type>(idx); \
      } \
      if (fallback) { \
        return fallback.value(); \
      } \
      throw std::runtime_error("Cannot convert \"" + std::string(str) + "\" to " #Clazz); \
    } \
    template<typename T, typename = typename std::enable_if<std::is_base_of<typename T::detail, detail>::value>::type> \
    [[maybe_unused]] constexpr T cast() const { \
      if (0 <= value_ && value_ < T::length) { \
        return static_cast<typename T::Type>(value_); \
      } \
      return {}; \
    }

#define SMART_ENUM_OUT_OF_CLASS_DEFINITIONS(Clazz) \
  [[maybe_unused]] inline constexpr std::string_view toStringView(Clazz::Type a) { \
    int index = static_cast<int>(a); \
    if (0 <= index && index < Clazz::length) { \
      return Clazz::values.at(index); \
    } \
    return "Unknown value for " #Clazz;                                                   \
  }

/**
 * These macros provide an encapsulation of enum-like behavior offering the following:
 *  - switch safety: the compiler can detect if some enum cases are not handled
 *  - string conversion: convert between enum instances and their string representations (toString, parse)
 *  - validity: check if it contains a value that is an invalid enum value
 *  - extensibility: extend an enum with new values and safely* cast from derived to base
 *                   (* "safely" here means that there is no casting between unrelated enums)
 *  - reflection: access the set of all string representations
 */

#define SMART_ENUM(Clazz, ...) \
  struct Clazz { \
    using Base = ::org::apache::nifi::minifi::utils::EnumBase; \
    enum Type : int { \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__)) \
    }; \
    SMART_ENUM_BODY(Clazz, __VA_ARGS__) \
  }; \
  SMART_ENUM_OUT_OF_CLASS_DEFINITIONS(Clazz)

#define SMART_ENUM_EXTEND(Clazz, base, base_fields, ...) \
  struct Clazz { \
    using Base = base; \
    enum Type : int { \
      FOR_EACH(INCLUDE_BASE_FIELD, COMMA, base_fields), \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__)) \
    }; \
    static_assert((COUNT base_fields) == Base::length, "Must enumerate all base instance values"); \
    SMART_ENUM_BODY(Clazz, __VA_ARGS__) \
  }; \
  SMART_ENUM_OUT_OF_CLASS_DEFINITIONS(Clazz)

struct EnumBase {
  enum Type {};
  static constexpr int length = 0;
  static constexpr std::array<std::string_view, 0> values = {};
  struct detail {};
};

}  // namespace org::apache::nifi::minifi::utils
