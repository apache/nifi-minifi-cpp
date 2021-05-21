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

#include <functional>
#include <string>
#include <type_traits>

#include "../TestBase.h"
#include "utils/GeneralUtils.h"

namespace utils = org::apache::nifi::minifi::utils;

static_assert(std::is_same<decltype(utils::make_unique<char16_t>()), std::unique_ptr<char16_t>>::value, "utils::make_unique type must be correct");

TEST_CASE("GeneralUtils::make_unique", "[make_unique]") {
  const auto pstr = utils::make_unique<std::string>("test string");
  REQUIRE("test string" == *pstr);
}

// intdiv_ceil
static_assert(0 == utils::intdiv_ceil(0, 1), "");
static_assert(0 == utils::intdiv_ceil(0, 2), "");
static_assert(1 == utils::intdiv_ceil(1, 2), "");
static_assert(1 == utils::intdiv_ceil(1, 3), "");
static_assert(1 == utils::intdiv_ceil(3, 3), "");
static_assert(2 == utils::intdiv_ceil(4, 3), "");
static_assert(2 == utils::intdiv_ceil(5, 3), "");
static_assert(0 == utils::intdiv_ceil(-1, 3), "");
static_assert(-1 == utils::intdiv_ceil(-3, 3), "");
static_assert(-1 == utils::intdiv_ceil(-4, 3), "");
static_assert(2 == utils::intdiv_ceil(-4, -3), "");
static_assert(2 == utils::intdiv_ceil(-5, -3), "");
static_assert(0 == utils::intdiv_ceil(1, -3), "");
static_assert(-1 == utils::intdiv_ceil(5, -3), "");
static_assert(3 == utils::intdiv_ceil(6, 2), "");
static_assert(-3 == utils::intdiv_ceil(-6, 2), "");
static_assert(-3 == utils::intdiv_ceil(6, -2), "");
static_assert(3 == utils::intdiv_ceil(-6, -2), "");
static_assert(0 == utils::intdiv_ceil(0, -10), "");

template<int N, int D, typename = void>
struct does_compile : std::false_type {};

template<int N, int D>
struct does_compile<N, D,
    // we must force evaluation so decltype won't do
    typename std::enable_if<(utils::intdiv_ceil(N, D), true)>::type> : std::true_type {};

static_assert(does_compile<2, 3>::value, "does_compile should work");
static_assert(!does_compile<1, 0>::value, "constexpr division by zero shouldn't compile");


TEST_CASE("GeneralUtils::exchange", "[exchange]") {
  int a = 1;
  int b = 2;
  a = utils::exchange(b, 0);
  REQUIRE(2 == a);
  REQUIRE(0 == b);
}

static_assert(std::is_same<decltype(utils::void_t<char16_t>()), void>::value, "utils::void_t single arg must work");
static_assert(std::is_same<decltype(utils::void_t<int, double, bool, void, char16_t>()), void>::value, "utils::void_t multi arg must work");

TEST_CASE("GeneralUtils::invoke pointer to member function", "[invoke memfnptr]") {
  const int result{0xc1ca};
  struct Tester {
    bool called{};
    int memfn(const int arg) {
      REQUIRE(42 == arg);
      called = true;
      return result;
    }
  };

  // normal
  REQUIRE(result == utils::invoke(&Tester::memfn, Tester{}, 42));

  // reference_wrapper
  Tester t2;
  const auto ref_wrapper = std::ref(t2);
  REQUIRE(result == utils::invoke(&Tester::memfn, ref_wrapper, 42));
  REQUIRE(t2.called);

  // pointer
  Tester t3;
  REQUIRE(result == utils::invoke(&Tester::memfn, &t3, 42));
  REQUIRE(t3.called);
}

TEST_CASE("GeneralUtils::invoke pointer to data member", "[invoke data member]") {
  struct Times2 {
    int value;
    explicit Times2(const int i) :value{i * 2} {}
  };

  // normal
  REQUIRE(24 == utils::invoke(&Times2::value, Times2{12}));

  // reference_wrapper
  Times2 t2{42};
  const auto ref_wrapper = std::ref(t2);
  REQUIRE(84 == utils::invoke(&Times2::value, ref_wrapper));

  // pointer
  Times2 t3{0};
  REQUIRE(0 == utils::invoke(&Times2::value, &t3));
}

namespace {
bool free_function(const bool b) { return b; }
}  // namespace

TEST_CASE("GeneralUtils::invoke FunctionObject", "[invoke function object]") {
  REQUIRE(true == utils::invoke(&free_function, true));
  REQUIRE(false == utils::invoke(&free_function, false));

  const auto int_timesn = [](const int i) { return 3 * i; };

  // invoking lambda
  REQUIRE(60 == utils::invoke(int_timesn, 20));
}

TEST_CASE("GeneralUtils::dereference", "[dereference]") {
  const int a = 42;
  const auto* const pa = &a;
  REQUIRE(42 == utils::dereference(pa));
  REQUIRE(&a == &utils::dereference(pa));
}
