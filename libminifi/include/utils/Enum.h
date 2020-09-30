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
#define FOR_EACH_1(fn, delim, _1) fn(_1)
#define FOR_EACH_2(fn, delim, _1, _2) fn(_1) delim() fn(_2)
#define FOR_EACH_3(fn, delim, _1, _2, _3) fn(_1) delim() fn(_2) delim() fn(_3)
#define FOR_EACH_4(fn, delim, _1, _2, _3, _4) \
  fn(_1) delim() fn(_2) delim() fn(_3) delim() fn(_4)
#define FOR_EACH_5(fn, delim, _1, _2, _3, _4, _5) \
  fn(_1) delim() fn(_2) delim() fn(_3) delim() fn(_4) delim() fn(_5)
#define FOR_EACH_6(fn, delim, _1, _2, _3, _4, _5, _6) \
  fn(_1) delim() fn(_2) delim() fn(_3) delim() fn(_4) delim() \
  fn(_5) delim() fn(_6)
#define FOR_EACH_7(fn, delim, _1, _2, _3, _4, _5, _6, _7) \
  fn(_1) delim() fn(_2) delim() fn(_3) delim() fn(_4) delim() \
  fn(_5) delim() fn(_6) delim() fn(_7)
#define FOR_EACH_8(fn, delim, _1, _2, _3, _4, _5, _6, _7, _8) \
  fn(_1) delim() fn(_2) delim() fn(_3) delim() fn(_4) delim() \
  fn(_5) delim() fn(_6) delim() fn(_7) delim() fn(_8)

#define FIRST_(a, b) a
#define FIRST(x, ...) FIRST_ x
#define SECOND_(a, b) b
#define SECOND(x, ...) SECOND_ x
#define NOTHING()

#define INCLUDE_BASE_FIELD(x) \
  x = Base::x

#define SMART_ENUM_BODY(name, ...) \
    static constexpr int length = Base::length + COUNT(__VA_ARGS__); \
    friend const char* toString(Type a) { \
      return toStringImpl(a, #name); \
    } \
    static Type parse(const char* str) { \
      for (int idx = 0; idx < length; ++idx) { \
        if (std::strcmp(str, toString(static_cast<Type>(idx))) == 0) \
          return static_cast<Type>(idx); \
      } \
      throw std::runtime_error(std::string("Unknown instance in enum \"" #name "\" : \"") + str + "\""); \
    } \
    template<typename T, Type value, typename = typename std::enable_if<std::is_base_of<T, name>::value>::type, typename = decltype(T::template fromInt<static_cast<int>(value)>())> \
    static typename T::Type cast() { \
      return T::template fromInt<static_cast<int>(value)>(); \
    } \
    template<typename T, typename = typename std::enable_if<std::is_base_of<T, name>::value>::type> \
    static typename T::Type cast(Type value) { \
      return T::fromInt(static_cast<int>(value)); \
    } \
    template<int x, typename = typename std::enable_if<(0 <= x && x < length)>::type> \
    static Type fromInt() { \
      return static_cast<Type>(x); \
    } \
    static Type fromInt(int x) { \
      if (0 <= x && x < length) { \
        return static_cast<Type>(x); \
      } \
      throw std::out_of_range("Cannot convert " + std::to_string(x) + " to enum \"" + #name + "\""); \
    } \
    \
   protected: \
    static const char* toStringImpl(Type a, const char* DerivedName) { \
      static constexpr const char* values[]{ \
        FOR_EACH(SECOND, COMMA, (__VA_ARGS__)) \
      }; \
      int index = static_cast<int>(a); \
      if (Base::length <= index && index < length) { \
        return values[index - Base::length]; \
      } \
      return Base::toStringImpl(static_cast<Base::Type>(a), DerivedName); \
    }

#define SMART_ENUM(name, ...) \
  struct name : ::org::apache::nifi::minifi::utils::EnumBase { \
    using Base = ::org::apache::nifi::minifi::utils::EnumBase; \
    enum Type { \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__)) \
    }; \
    SMART_ENUM_BODY(name, __VA_ARGS__) \
  };

#define SMART_ENUM_EXTEND(name, base, base_fields, ...) \
  struct name : public base { \
    using Base = base; \
    enum Type { \
      FOR_EACH(INCLUDE_BASE_FIELD, COMMA, base_fields), \
      FOR_EACH(FIRST, COMMA, (__VA_ARGS__)) \
    }; \
    static_assert((COUNT base_fields) == Base::length, "Must enumerate all base instance values"); \
    SMART_ENUM_BODY(name, __VA_ARGS__) \
  };

struct EnumBase {
  enum Type {};
  static constexpr int length = 0;
 protected:
  static const char* toStringImpl(Type a, const char* DerivedName) {
    throw std::runtime_error(std::string("Cannot stringify unknown instance in enum \"") + DerivedName + "\" : \""
          + std::to_string(static_cast<int>(a)) + "\"");
  }
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
