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

TEST_CASE("GeneralUtils::dereference", "[dereference]") {
  const int a = 42;
  const auto* const pa = &a;
  REQUIRE(42 == utils::dereference(pa));
  REQUIRE(&a == &utils::dereference(pa));
}

TEST_CASE("GeneralUtils::dereference on optional", "[dereference optional]") {
  const std::optional opt_int{42};
  REQUIRE(42 == *opt_int);
  REQUIRE(42 == utils::dereference(opt_int));
  REQUIRE(&*opt_int == &utils::dereference(opt_int));
}
