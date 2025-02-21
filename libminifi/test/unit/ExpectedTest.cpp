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
#define EXTENSION_LIST ""  // NOLINT(cppcoreguidelines-macro-usage)
#include <memory>
#include <string_view>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/expected.h"
#include "utils/gsl.h"

namespace utils = org::apache::nifi::minifi::utils;

// shamelessly copied from https://github.com/TartanLlama/expected/blob/master/tests/extensions.cpp (License: CC0)
TEST_CASE("expected transform", "[expected][transform]") {
  auto mul2 = [](int a) { return a * 2; };
  auto ret_void = [](int) {};

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::transform(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::transform(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::transform(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::transform(mul2);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::transform(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::transform(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::transform(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::transform(mul2);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::transform(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::transform(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::transform(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::transform(ret_void);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::transform(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::transform(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::transform(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::transform(ret_void);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same_v<decltype(ret), nonstd::expected<void, int>>));
  }


  // mapping functions which return references
  {
    nonstd::expected<int, int> e(42);
    auto ret = e | utils::transform([](int& i) -> int& { return i; });
    REQUIRE(ret);
    REQUIRE(ret == 42);
  }
}

TEST_CASE("expected andThen", "[expected][andThen]") {
  auto succeed = [](int) { return nonstd::expected<int, int>(21 * 2); };
  auto fail = [](int) { return nonstd::expected<int, int>(nonstd::unexpect, 17); };

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::andThen(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::andThen(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::andThen(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::andThen(succeed);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::andThen(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::andThen(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::andThen(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::andThen(fail);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::andThen(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::andThen(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::andThen(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::andThen(succeed);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::andThen(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::andThen(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::andThen(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::andThen(fail);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }
}

TEST_CASE("expected orElse", "[expected][orElse]") {
  using eptr = std::unique_ptr<int>;
  auto succeed = [](int) { return nonstd::expected<int, int>(21 * 2); };
  auto succeedptr = [](eptr) { return nonstd::expected<int, eptr>(21*2); };
  auto fail =    [](int) { return nonstd::expected<int, int>(nonstd::unexpect, 17); };
  auto efail =   [](eptr e) { *e = 17; return nonstd::expected<int, eptr>(nonstd::unexpect, std::move(e)); };
  auto failvoid = [](int) {};
  auto failvoidptr = [](const eptr&) { /* don't consume */};
  auto consumeptr = [](eptr) {};
  auto make_u_int = [](int n) { return std::make_unique<int>(n); };

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::orElse(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::orElse(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::orElse(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    nonstd::expected<int, eptr> e = 21;
    auto ret = std::move(e) | utils::orElse(succeedptr);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::orElse(succeed);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::orElse(fail);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::orElse(fail);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::orElse(fail);
    REQUIRE(ret);
    REQUIRE(ret == 21);
  }


  {
    nonstd::expected<int, eptr> e = 21;
    auto ret = std::move(e) | utils::orElse(efail);
    REQUIRE(ret);
    REQUIRE(ret == 21);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::orElse(fail);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::orElse(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::orElse(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, eptr> e(nonstd::unexpect, make_u_int(21));
    auto ret = std::move(e) | utils::orElse(succeedptr);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(succeed);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::orElse(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::orElse(failvoid);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::orElse(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::orElse(failvoid);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(failvoid);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, eptr> e(nonstd::unexpect, make_u_int(21));
    auto ret = std::move(e) | utils::orElse(failvoidptr);
    REQUIRE(!ret);
    REQUIRE(*ret.error() == 21);
  }

  {
    nonstd::expected<int, eptr> e(nonstd::unexpect, make_u_int(21));
    auto ret = std::move(e) | utils::orElse(consumeptr);
    REQUIRE(!ret);
    REQUIRE(ret.error() == nullptr);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(fail);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(failvoid);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }
}

TEST_CASE("expected valueOrElse", "[expected][valueOrElse]") {
  using namespace std::literals;  // NOLINT
  nonstd::expected<int, std::string> ex{nonstd::unexpect, "hello"};
  REQUIRE(42 == (ex | utils::valueOrElse([] { return 42; })));
  REQUIRE_THROWS_AS(ex | utils::valueOrElse([]{ throw std::exception(); }), std::exception);
  REQUIRE(gsl::narrow<int>("hello"sv.size()) == (ex | utils::valueOrElse([](const std::string& err) { return gsl::narrow<int>(err.size()); })));
  REQUIRE_THROWS_AS(ex | utils::valueOrElse([](std::string){ throw std::exception(); }), std::exception);  // NOLINT(performance-unnecessary-value-param)
  REQUIRE_THROWS_AS(ex | utils::valueOrElse([](const std::string&) -> int { throw std::exception(); }), std::exception);
  REQUIRE_THROWS_WITH(std::move(ex) | utils::valueOrElse([](std::string&& error) -> int { throw std::runtime_error(error); }), "hello");
}

TEST_CASE("expected transformError", "[expected][transformError]") {
  auto mul2 = [](int a) { return a * 2; };

  {
    nonstd::expected<int, int> e = nonstd::make_unexpected(21);
    auto ret = e | utils::transformError(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 42);
  }

  {
    nonstd::expected<size_t, std::string> e = nonstd::make_unexpected("sajt");
    auto ret = e | utils::transformError([](const std::string& error) -> size_t { return error.length(); });
    REQUIRE(!ret);
    REQUIRE(ret.error() == 4);
  }

  {
    const nonstd::expected<int, int> e = nonstd::make_unexpected(21);
    auto ret = e | utils::transformError(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 42);
  }

  {
    nonstd::expected<int, int> e = nonstd::make_unexpected(21);
    auto ret = std::move(e) | utils::transformError(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 42);
  }

  {
    const nonstd::expected<int, int> e = nonstd::make_unexpected(21);
    auto ret = std::move(e) | utils::transformError(mul2);  // NOLINT(performance-move-const-arg)
    REQUIRE(!ret);
    REQUIRE(ret.error() == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::transformError(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::transformError(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    const auto mutable_rvalue_fn = []{ return nonstd::expected<int, int>{21}; };
    auto ret = mutable_rvalue_fn() | utils::transformError(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::transformError(mul2);  // NOLINT(performance-move-const-arg)
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }
}

TEST_CASE("expected toOptional") {
  nonstd::expected<int, std::string> unexpected{nonstd::unexpect, "hello"};
  nonstd::expected<int, std::string> expected{5};

  std::optional<int> res1 = std::move(unexpected) | utils::toOptional();
  std::optional<int> res2 = std::move(expected) | utils::toOptional();

  CHECK(res1 == std::nullopt);
  CHECK(res2 == 5);
}

TEST_CASE("expected orThrow") {
  nonstd::expected<int, std::string> unexpected{nonstd::unexpect, "hello"};
  nonstd::expected<int, std::string> expected{5};

  REQUIRE_THROWS_WITH(std::move(unexpected) | utils::orThrow("should throw"), "should throw, but got hello");
  CHECK((expected | utils::orThrow("should be 5")) == 5);
}
