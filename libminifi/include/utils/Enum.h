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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#define COMMA(...) ,

#define _PICK(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define COUNT(...) \
  _PICK(__VA_ARGS__, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define _CONCAT(a, b) a ## b
#define CONCAT(a, b) _CONCAT(a, b)

#define CALL(Fn, ...) Fn (__VA_ARGS__)
#define SPREAD(...) __VA_ARGS__

#define FOR_EACH(fn, delim, ARGS, EXTRA) \
  CALL(CONCAT(FOR_EACH_, COUNT ARGS), fn, delim, EXTRA, SPREAD ARGS)

#define FOR_EACH_0(...)
#define FOR_EACH_1(fn, delim, EXTRA, _1) fn(_1, EXTRA)
#define FOR_EACH_2(fn, delim, EXTRA, _1, _2) fn(_1, EXTRA) delim() fn(_2, EXTRA)
#define FOR_EACH_3(fn, delim, EXTRA, _1, _2, _3) fn(_1, EXTRA) delim() fn(_2, EXTRA) delim() fn(_3, EXTRA)
#define FOR_EACH_4(fn, delim, EXTRA, _1, _2, _3, _4) \
  fn(_1, EXTRA) delim() fn(_2, EXTRA) delim() fn(_3, EXTRA) delim() fn(_4, EXTRA)
#define FOR_EACH_5(fn, delim, EXTRA, _1, _2, _3, _4, _5) \
  fn(_1, EXTRA) delim() fn(_2, EXTRA) delim() fn(_3, EXTRA) delim() fn(_4, EXTRA) delim() fn(_5, EXTRA)
#define FOR_EACH_6(fn, delim, EXTRA, _1, _2, _3, _4, _5, _6) \
  fn(_1, EXTRA) delim() fn(_2, EXTRA) delim() fn(_3, EXTRA) delim() fn(_4, EXTRA) delim() \
  fn(_5, EXTRA) delim() fn(_6, EXTRA)
#define FOR_EACH_7(fn, delim, EXTRA, _1, _2, _3, _4, _5, _6, _7) \
  fn(_1, EXTRA) delim() fn(_2, EXTRA) delim() fn(_3, EXTRA) delim() fn(_4, EXTRA) delim() \
  fn(_5, EXTRA) delim() fn(_6, EXTRA) delim() fn(_7, EXTRA)
#define FOR_EACH_8(fn, delim, EXTRA, _1, _2, _3, _4, _5, _6, _7, _8) \
  fn(_1, EXTRA) delim() fn(_2, EXTRA) delim() fn(_3, EXTRA) delim() fn(_4, EXTRA) delim() \
  fn(_5, EXTRA) delim() fn(_6, EXTRA) delim() fn(_7, EXTRA) delim() fn(_8, EXTRA)

#define _FIRST(a, b) a
#define FIRST(x, ...) _FIRST x
#define _SECOND(a, b) b
#define SECOND(x, ...) _SECOND x
#define NOTHING()

#define ACCESS(x, ...) \
  Type::FIRST(x)

#define HEAD(x, ...) x

#define SMART_ENUM(name, ...) \
  struct name { \
    enum Type { \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__), ()) \
    }; \
    static constexpr std::size_t length = COUNT(__VA_ARGS__); \
    static constexpr Type keys[]{ \
      FOR_EACH(ACCESS, COMMA, (__VA_ARGS__), ()) \
    }; \
    static constexpr const char* values[]{ \
      FOR_EACH(SECOND, COMMA, (__VA_ARGS__), ()) \
    }; \
    friend const char* toString(Type a) { \
      int index = static_cast<int>(a); \
      if (0 <= index && index < length) { \
        return values[index]; \
      } \
      throw std::runtime_error(std::string("Cannot stringify unknown instance in enum \"" #name "\" : \"") \
          + std::to_string(index) + "\""); \
    } \
    static Type parse(const char* str) { \
      for (std::size_t idx = 0; idx < sizeof(keys) / sizeof(keys[0]); ++idx) { \
        if (std::strcmp(str, toString(keys[idx])) == 0) \
          return keys[idx]; \
      } \
      throw std::runtime_error(std::string("Unknown instance in enum \"" #name "\" : \"") + str + "\""); \
    } \
    template<int x, typename = typename std::enable_if<(0 <= x && x < length)>::type> \
    static Type fromInt() { \
      return Type(x); \
    } \
    static Type fromInt(int x) { \
      if (0 <= x && x < length) { \
        return Type(x); \
      } \
      throw std::out_of_range("Cannot convert " + std::to_string(x) + " to enum \"" + #name + "\""); \
    } \
  };

#define INCLUDE_BASE_FIELD(x, extra) \
  x = HEAD extra::x

#define SMART_ENUM_EXTEND(name, base, base_fields, ...) \
  struct name : public base { \
    enum Type { \
      FOR_EACH(INCLUDE_BASE_FIELD, COMMA, base_fields, (base)), \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__), ()) \
    }; \
    static constexpr std::size_t length = base::length + COUNT(__VA_ARGS__); \
    static_assert((COUNT base_fields) == base::length, "Must enumerate all base instance values"); \
    static constexpr Type keys[]{ \
      FOR_EACH(ACCESS, COMMA, (__VA_ARGS__), ()) \
    }; \
    static constexpr const char* values[]{ \
      FOR_EACH(SECOND, COMMA, (__VA_ARGS__), ()) \
    }; \
    friend const char* toString(Type a) { \
      int index = static_cast<int>(a); \
      if (base::length <= index && index < length) { \
        return values[index - base::length]; \
      } \
      return toString(base::Type(a)); \
    } \
    static Type parse(const char* str) { \
      for (std::size_t idx = 0; idx < sizeof(keys)/sizeof(keys[0]); ++idx) { \
        if (std::strcmp(str, toString(keys[idx])) == 0) \
          return keys[idx]; \
      } \
      return Type(base::parse(str)); \
    } \
    template<typename T, Type value, typename = decltype(T::template fromInt<static_cast<int>(value)>())> \
    static typename T::Type cast() { \
      return T::template fromInt<static_cast<int>(value)>(); \
    } \
    template<typename T> \
    static typename T::Type cast(Type value) { \
      return T::fromInt(static_cast<int>(value)); \
    } \
    template<int x, typename = typename std::enable_if<(0 <= x && x < length)>::type> \
    static Type fromInt() { \
      return Type(x); \
    } \
    static Type fromInt(int x) { \
      if (0 <= x && x < length) { \
        return Type(x); \
      } \
      throw std::out_of_range("Cannot convert " + std::to_string(x) + " to enum \"" + #name + "\""); \
    } \
  };

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
