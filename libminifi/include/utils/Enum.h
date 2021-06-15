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

#include <string>
#include <cstring>
#include <set>

#include "OptionalUtils.h"
#include "StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#define COMMA(...) ,
#define MSVC_HACK(x) x

#define PICK_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define COUNT(...) \
  MSVC_HACK(PICK_(__VA_ARGS__, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0))

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define CALL(Fn, ...) MSVC_HACK(Fn(__VA_ARGS__))
#define SPREAD(...) __VA_ARGS__

#define FOR_EACH(fn, delim, ARGS) \
  CALL(CONCAT(FOR_EACH_, COUNT ARGS), fn, delim, SPREAD ARGS)

#define FOR_EACH_0(...)
#define FOR_EACH_1(fn, delim, _1) \
  fn(_1)
#define FOR_EACH_2(fn, delim, _1, _2) \
  fn(_1) delim() fn(_2)
#define FOR_EACH_3(fn, delim, _1, _2, _3) \
  fn(_1) delim() FOR_EACH_2(fn, delim, _2, _3)
#define FOR_EACH_4(fn, delim, _1, _2, _3, _4) \
  fn(_1) delim() FOR_EACH_3(fn, delim, _2, _3, _4)
#define FOR_EACH_5(fn, delim, _1, _2, _3, _4, _5) \
  fn(_1) delim() FOR_EACH_4(fn, delim, _2, _3, _4, _5)
#define FOR_EACH_6(fn, delim, _1, _2, _3, _4, _5, _6) \
  fn(_1) delim() FOR_EACH_5(fn, delim, _2, _3, _4, _5, _6)
#define FOR_EACH_7(fn, delim, _1, _2, _3, _4, _5, _6, _7) \
  fn(_1) delim() FOR_EACH_6(fn, delim, _2, _3, _4, _5, _6, _7)
#define FOR_EACH_8(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8) \
  fn(_1) delim() FOR_EACH_7(fn, delim, _2, _3, _4, _5, _6, _7, _8)
#define FOR_EACH_9(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9) \
  fn(_1) delim() FOR_EACH_8(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9)
#define FOR_EACH_10(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10) \
  fn(_1) delim() FOR_EACH_9(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9, _10)
#define FOR_EACH_11(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11) \
  fn(_1) delim() FOR_EACH_10(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11)
#define FOR_EACH_12(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12) \
  fn(_1) delim() FOR_EACH_11(fn, delim, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12)

#define FIRST_(a, b) a
#define FIRST(x, ...) FIRST_ x
#define SECOND_(a, b) b
#define SECOND(x, ...) SECOND_ x
#define NOTHING()

#define INCLUDE_BASE_FIELD(x) \
  x = Base::x

#define SMART_ENUM_BODY(Clazz, ...) \
    constexpr Clazz(Type value = static_cast<Type>(-1)) : value_{value} {} \
    explicit Clazz(const std::string& str) : value_{parse(str.c_str()).value_} {} \
    explicit Clazz(const char* str) : value_{parse(str).value_} {} \
   private: \
    Type value_; \
   public: \
    Type value() const { \
      return value_; \
    } \
    struct detail : Base::detail { \
      static std::set<std::string> values() { \
        static constexpr const char* ownValues[]{ \
          FOR_EACH(SECOND, COMMA, (__VA_ARGS__)) \
        }; \
        std::set<std::string> values = Base::detail::values(); \
        for (auto value : ownValues) { \
          values.emplace(value); \
        } \
        return values; \
      } \
      static const char* toStringImpl(Type a, const char* DerivedName) { \
        static constexpr const char* values[]{ \
          FOR_EACH(SECOND, COMMA, (__VA_ARGS__)) \
        }; \
        int index = static_cast<int>(a); \
        if (Base::length <= index && index < length) { \
          return values[index - Base::length]; \
        } \
        return Base::detail::toStringImpl(static_cast<Base::Type>(a), DerivedName); \
      } \
    }; \
    static constexpr int length = Base::length + COUNT(__VA_ARGS__); \
    friend const char* toString(Type a) { \
      return detail::toStringImpl(a, #Clazz); \
    } \
    const char* toString() const { \
      return detail::toStringImpl(value_, #Clazz); \
    } \
    const char* toStringOr(const char* fallback) const { \
      if (*this) { \
        return toString(); \
      } \
      return fallback; \
    } \
    static std::set<std::string> values() { \
      return detail::values(); \
    } \
    friend bool operator==(Clazz lhs, Clazz rhs) { \
      return lhs.value_ == rhs.value_; \
    } \
    friend bool operator!=(Clazz lhs, Clazz rhs) { \
      return lhs.value_ != rhs.value_; \
    } \
    friend bool operator<(Clazz lhs, Clazz rhs) { \
      return lhs.value_ < rhs.value_;\
    } \
    explicit operator bool() const { \
      int idx = static_cast<int>(value_); \
      return 0 <= idx && idx < length; \
    } \
    static Clazz parse(const char* str, const ::org::apache::nifi::minifi::utils::optional<Clazz>& fallback = {}, bool caseSensitive = true) { \
      for (int idx = 0; idx < length; ++idx) { \
        if (::org::apache::nifi::minifi::utils::StringUtils::equals(str, detail::toStringImpl(static_cast<Type>(idx), #Clazz), caseSensitive)) \
          return static_cast<Type>(idx); \
      } \
      if (fallback) { \
        return fallback.value(); \
      } \
      throw std::runtime_error(std::string("Cannot convert \"") + str + "\" to " #Clazz); \
    } \
    template<typename T, typename = typename std::enable_if<std::is_base_of<typename T::detail, detail>::value>::type> \
    T cast() const { \
      if (0 <= value_ && value_ < T::length) { \
        return static_cast<typename T::Type>(value_); \
      } \
      return {}; \
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
    enum Type { \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__)) \
    }; \
    SMART_ENUM_BODY(Clazz, __VA_ARGS__) \
  };

#define SMART_ENUM_EXTEND(Clazz, base, base_fields, ...) \
  struct Clazz { \
    using Base = base; \
    enum Type { \
      FOR_EACH(INCLUDE_BASE_FIELD, COMMA, base_fields), \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__)) \
    }; \
    static_assert((COUNT base_fields) == Base::length, "Must enumerate all base instance values"); \
    SMART_ENUM_BODY(Clazz, __VA_ARGS__) \
  };

struct EnumBase {
  enum Type {};
  static constexpr int length = 0;
  struct detail {
    static std::set<std::string> values() {
      return {};
    }
    static const char* toStringImpl(Type a, const char* DerivedName) {
      throw std::runtime_error(std::string("Cannot stringify unknown instance in enum \"") + DerivedName + "\" : \""
                               + std::to_string(static_cast<int>(a)) + "\"");
    }
  };
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
