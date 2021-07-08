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

#include "../TestBase.h"
#include "utils/OptionalUtils.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("optional map", "[optional map]") {
  const auto test1 = utils::make_optional(6) | utils::map([](const int i) { return i * 2; });
  REQUIRE(12 == test1.value());

  const auto test2 = utils::optional<int>{} | utils::map([](const int i) { return i * 2; });
  REQUIRE(!test2.has_value());
}

TEST_CASE("optional flatMap", "[optional flat map]") {
  const auto make_intdiv_noremainder = [](const int denom) {
    return [denom](const int num) { return num % denom == 0 ? utils::make_optional(num / denom) : utils::optional<int>{}; };
  };

  const auto test1 = utils::make_optional(6) | utils::flatMap(make_intdiv_noremainder(3));
  REQUIRE(2 == test1.value());

  const auto const_lval_func = make_intdiv_noremainder(4);
  const auto test2 = utils::optional<int>{} | utils::flatMap(const_lval_func);
  REQUIRE(!test2.has_value());

  auto mutable_lval_func = make_intdiv_noremainder(3);
  const auto test3 = utils::make_optional(7) | utils::flatMap(mutable_lval_func);
  REQUIRE(!test3.has_value());
}

TEST_CASE("optional orElse", "[optional or else]") {
  const auto opt_7 = [] { return utils::make_optional(7); };
  const auto test1 = utils::make_optional(6) | utils::orElse(opt_7);
  const auto test2 = utils::optional<int>{} | utils::orElse(opt_7);
  const auto test3 = utils::make_optional(3) | utils::orElse([]{});
  const auto test4 = utils::optional<int>{} | utils::orElse([]{});
  struct ex : std::exception {};

  REQUIRE(6 == test1.value());
  REQUIRE(7 == test2.value());
  REQUIRE(3 == test3.value());
  REQUIRE(!test4);
  REQUIRE_THROWS_AS(utils::optional<bool>{} | utils::orElse([]{ throw ex{}; }), ex);
}
