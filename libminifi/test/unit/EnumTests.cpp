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
#include "../Catch.h"

// we need this instead of void_t in GeneralUtils because of GCC4.8
template<typename ...>
struct make_void {
  using type = void;
};

#define CHECK_COMPILE(name, clazz, expr, result) \
  template<typename, typename = void> \
  struct does_compile ## name : std::false_type {};\
  template<typename T> \
  struct does_compile ## name<T, typename make_void<decltype(std::declval<T>().template expr)>::type> : std::true_type {}; \
  static_assert(does_compile ## name<clazz>::value == result, "");

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

CHECK_COMPILE(_1, B, cast<A>(), true)
CHECK_COMPILE(_2, C, cast<A>(), true)
CHECK_COMPILE(_3, C, cast<B>(), true)

// casting to unrelated fails
CHECK_COMPILE(_7, B, cast<Unrelated>(), false)
CHECK_COMPILE(_8, B, cast<Unrelated>(), false)

}  // namespace test

TEST_CASE("Enum checks") {
  REQUIRE(!A{});
  REQUIRE(!B{});
  REQUIRE(A{A::Type(0)});
  REQUIRE(B{B::Type(1)});

  REQUIRE(A::values == std::array<std::string_view, 2>{"zero", "one"});
  REQUIRE(B::values == std::array<std::string_view, 3>{"zero", "one", "two"});
  REQUIRE(C::values == std::array<std::string_view, 4>{"zero", "one", "two", "three"});

  REQUIRE_THROWS(A::parse("not_any"));
  REQUIRE(!A::parse("not_any", A{}));
  REQUIRE(A::parse("zero") == A::_0);
  REQUIRE(B::parse("zero") == B::_0);
  REQUIRE(C::parse("one") == C::_1);
  REQUIRE(C::parse("three") == C::_3);
  REQUIRE_THROWS(C::parse("nada"));
  REQUIRE(!C::parse("nada", C{}));
  REQUIRE(!C::parse("ThRee", C{}));
  REQUIRE(C::parse("ThRee", {}, false) == C::_3);

  REQUIRE(A{A::_0}.toString() == std::string{"zero"});
  REQUIRE(toString(A::_0) == std::string{"zero"});
  REQUIRE(B{B::_0}.toString() == std::string{"zero"});
  REQUIRE(toString(B::_0) == std::string{"zero"});
  REQUIRE(toString(B::_2) == std::string{"two"});
  REQUIRE(toString(C::_1) == std::string{"one"});
  REQUIRE(toString(C::_3) == std::string{"three"});
  REQUIRE(A{}.toStringOr("fallback") == std::string{"fallback"});
  REQUIRE_THROWS(toString(A::Type(55)));
  REQUIRE_THROWS(toString(C::Type(-1)));
  REQUIRE_THROWS(C{}.toString());

  REQUIRE(B{B::_0}.cast<A>() == A{A::_0});
  REQUIRE(C{C::_1}.cast<A>() == A::_1);
  REQUIRE(C{C::_2}.cast<B>() == B::_2);
  REQUIRE(!C{C::_3}.cast<B>());
  REQUIRE(!C{C::_3}.cast<A>());
  REQUIRE(!B{B::_2}.cast<A>());
}

TEST_CASE("Operations on Smart Enums are constexpr") {
  static constexpr auto c_values = C::values;
  CHECK(c_values == std::array<std::string_view, 4>{"zero", "one", "two", "three"});

  static constexpr bool zero_equals_zero{C::_0 == C::_0};  // NOLINT(misc-redundant-expression)
  static constexpr bool zero_does_not_equal_three{C::_0 != C::_3};
  static constexpr bool zero_is_less_than_two{C::_0 < C::_2};
  static constexpr bool one_is_valid{C::_1};
  CHECK((zero_equals_zero && zero_does_not_equal_three && zero_is_less_than_two && one_is_valid));

  static constexpr C zero_object{C::_0};
  static constexpr C::Type zero_type{zero_object.value()};
  CHECK(zero_type == 0);

  static constexpr C derived{C::_2};
  static constexpr B derived_as_base = derived.cast<B>();
  CHECK(toString(derived.value()) == toString(derived_as_base.value()));

  static constexpr std::string_view zero_name{toStringView(C::_0)};
  CHECK(zero_name == "zero");
}
