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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/OptionalUtils.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("optional transform", "[optional transform]") {
  const auto test1 = std::make_optional(6) | utils::transform([](const int i) { return i * 2; });
  REQUIRE(12 == test1.value());

  const auto test2 = std::optional<int>{} | utils::transform([](const int i) { return i * 2; });
  REQUIRE(!test2.has_value());
}

TEST_CASE("optional andThen", "[optional and then]") {
  const auto make_intdiv_noremainder = [](const int denom) {
    return [denom](const int num) { return num % denom == 0 ? std::make_optional(num / denom) : std::optional<int>{}; };
  };

  const auto test1 = std::make_optional(6) | utils::andThen(make_intdiv_noremainder(3));
  REQUIRE(2 == test1.value());

  const auto const_lval_func = make_intdiv_noremainder(4);
  const auto test2 = std::optional<int>{} | utils::andThen(const_lval_func);
  REQUIRE(!test2.has_value());

  auto mutable_lval_func = make_intdiv_noremainder(3);
  const auto test3 = std::make_optional(7) | utils::andThen(mutable_lval_func);
  REQUIRE(!test3.has_value());
}

TEST_CASE("optional orElse", "[optional or else]") {
  const auto opt_7 = [] { return std::make_optional(7); };
  const auto test1 = std::make_optional(6) | utils::orElse(opt_7);
  const auto test2 = std::optional<int>{} | utils::orElse(opt_7);
  const auto test3 = std::make_optional(3) | utils::orElse([]{});
  const auto test4 = std::optional<int>{} | utils::orElse([]{});
  struct ex : std::exception {};

  REQUIRE(6 == test1.value());
  REQUIRE(7 == test2.value());
  REQUIRE(3 == test3.value());
  REQUIRE(!test4);
  REQUIRE_THROWS_AS(std::optional<bool>{} | utils::orElse([]{ throw ex{}; }), ex);
}

TEST_CASE("optional valueOrElse", "[optional][valueOrElse]") {
  const auto seven = std::make_optional(7) | utils::valueOrElse([]() -> int { throw std::exception{}; });
  const auto test1 = std::make_optional(6) | utils::valueOrElse([] { return 49; });
  const auto test2 = std::optional<int>{} | utils::valueOrElse([] { return size_t{0}; });

  REQUIRE(7 == seven);
  REQUIRE(6 == test1);
  REQUIRE(0 == test2);
  REQUIRE_THROWS_AS(std::optional<int>{} | utils::valueOrElse([]() -> int { throw std::exception{}; }), std::exception);
}

TEST_CASE("optional filter", "[optional][filter]") {
  REQUIRE(7 == (std::make_optional(7) | utils::filter([](int i) { return i % 2 == 1; })).value());
  REQUIRE(std::nullopt == (std::make_optional(8) | utils::filter([](int i) { return i % 2 == 1; })));
}

TEST_CASE("optional toExpected") {
  std::optional<int> opt_with_value = 5;
  std::optional<int> opt_without_value = std::nullopt;

  nonstd::expected<int, std::error_code> expected_from_value_ec = opt_with_value | utils::toExpected(std::make_error_code(std::io_errc::stream));
  nonstd::expected<int, std::error_code> expected_from_null_opt_ec = opt_without_value | utils::toExpected(std::make_error_code(std::io_errc::stream));
  nonstd::expected<int, int> expected_from_value_int = opt_with_value | utils::toExpected(9);
  nonstd::expected<int, int> expected_from_null_opt_int = opt_without_value | utils::toExpected(9);

  CHECK(expected_from_value_ec == 5);
  CHECK(expected_from_value_int == 5);

  CHECK(expected_from_null_opt_ec.error() == std::make_error_code(std::io_errc::stream));
  CHECK(expected_from_null_opt_int.error() == 9);
}

TEST_CASE("optional orThrow") {
  std::optional<int> opt_with_value = 5;
  std::optional<int> opt_without_value = std::nullopt;

  REQUIRE_THROWS_WITH(opt_without_value | utils::orThrow("should throw"), "should throw");
  CHECK((opt_with_value | utils::orThrow("should be 5")) == 5);
}
