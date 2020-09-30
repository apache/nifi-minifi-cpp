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

#include <stdexcept>
#include <string>
#include <type_traits>
#include "utils/Enum.h"
#include "utils/GeneralUtils.h"
#include "catch.hpp"

using org::apache::nifi::minifi::utils::void_t;

#define ID(...) __VA_ARGS__
#define _STR(x) #x
#define STR(x) _STR(x)

#define COMPILE_ERROR(name, type, expr) \
  template<typename, typename = void>              \
  struct name : std::true_type {};\
  template<typename T> \
  struct name<T, void_t<decltype(T::ID expr)>> : std::false_type {}; \
  static_assert(name<type>::value, "Expression: " STR((type::ID expr)) " is not supposed to compile");

SMART_ENUM(A,
  (_0, "zero"),
  (_1, "one")
)

SMART_ENUM_EXTEND(B, A, (_0, _1),
  (_2, "two")
)

SMART_ENUM_EXTEND(C, B, (_0, _1, _2),
  (_3, "three")
)

SMART_ENUM(Unrelated,
  (a, "a"),
  (b, "b")
)

// static tests
namespace test {

COMPILE_ERROR(_1, A, (template fromInt<2>()))
COMPILE_ERROR(_2, B, (template cast<A, B::_2>()))
COMPILE_ERROR(_3, B, (template fromInt<3>()))
COMPILE_ERROR(_4, C, (template fromInt<4>()))
COMPILE_ERROR(_5, C, (template cast<A, C::_2>()))
COMPILE_ERROR(_6, C, (template cast<B, C::_3>()))

// casting to unrelated
COMPILE_ERROR(_7, B, (template cast<Unrelated, B::_0>()))
COMPILE_ERROR(_8, B, (template cast<Unrelated>(B::_0)))

}  // namespace test

TEST_CASE("Enum fromInt static") {
  REQUIRE(A::fromInt<0>() == A::_0);
  REQUIRE(B::fromInt<2>() == B::_2);
  REQUIRE(C::fromInt<3>() == C::_3);
}

TEST_CASE("Enum cast static") {
  REQUIRE((B::cast<A, B::_0>() == A::_0));
  REQUIRE((C::cast<A, C::_0>() == A::_0));
  REQUIRE((C::cast<B, C::_0>() == B::_0));
  REQUIRE((C::cast<B, C::_2>() == B::_2));
}

TEST_CASE("Enum runtime checks") {
  REQUIRE_THROWS(A::parse("not_any"));
  REQUIRE(A::parse("zero") == A::_0);
  REQUIRE(B::parse("zero") == B::_0);
  REQUIRE(C::parse("one") == C::_1);
  REQUIRE(C::parse("three") == C::_3);
  REQUIRE_THROWS(C::parse("nada"));

  REQUIRE(toString(A::_0) == std::string{"zero"});
  REQUIRE(toString(B::_0) == std::string{"zero"});
  REQUIRE(toString(B::_2) == std::string{"two"});
  REQUIRE(toString(C::_1) == std::string{"one"});
  REQUIRE(toString(C::_3) == std::string{"three"});
  REQUIRE_THROWS(toString(A::Type(55)));
  REQUIRE_THROWS(toString(C::Type(-1)));

  REQUIRE(A::fromInt(0) == A::_0);
  REQUIRE(B::fromInt(0) == B::_0);
  REQUIRE(B::fromInt(2) == B::_2);

  REQUIRE(B::cast<A>(B::_0) == A::_0);
  REQUIRE(C::cast<A>(C::_1) == A::_1);
  REQUIRE(C::cast<B>(C::_2) == B::_2);
  REQUIRE_THROWS(C::cast<B>(C::_3));
  REQUIRE_THROWS(C::cast<A>(C::_3));
  REQUIRE_THROWS(B::cast<A>(B::_2));
}
