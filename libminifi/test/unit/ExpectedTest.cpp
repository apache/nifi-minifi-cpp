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
#define EXTENSION_LIST ""
#include <string_view>
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/expected.h"
#include "utils/gsl.h"

namespace utils = org::apache::nifi::minifi::utils;

// shamelessly copied from https://github.com/TartanLlama/expected/blob/master/tests/extensions.cpp (License: CC0)
TEST_CASE("expected map", "[expected][map]") {
  auto mul2 = [](int a) { return a * 2; };
  auto ret_void = [](int) {};

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::map(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::map(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::map(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::map(mul2);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::map(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::map(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::map(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::map(mul2);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::map(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::map(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::map(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::map(ret_void);
    REQUIRE(ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::map(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::map(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::map(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::map(ret_void);
    REQUIRE(!ret);
    STATIC_REQUIRE(
        (std::is_same<decltype(ret), nonstd::expected<void, int>>::value));
  }


  // mapping functions which return references
  {
    nonstd::expected<int, int> e(42);
    auto ret = e | utils::map([](int& i) -> int& { return i; });
    REQUIRE(ret);
    REQUIRE(ret == 42);
  }
}

TEST_CASE("expected flatMap", "[expected][flatMap]") {
  auto succeed = [](int) { return nonstd::expected<int, int>(21 * 2); };
  auto fail = [](int) { return nonstd::expected<int, int>(nonstd::unexpect, 17); };

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::flatMap(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::flatMap(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::flatMap(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::flatMap(succeed);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = e | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = e | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::flatMap(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::flatMap(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::flatMap(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::flatMap(succeed);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = e | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::flatMap(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 21);
  }
}

TEST_CASE("expected orElse", "[expected][orElse]") {
  // commented out parts depend on https://github.com/martinmoene/expected-lite/pull/40 (waiting for release)
  // using eptr = std::unique_ptr<int>;
  auto succeed = [](int) { return nonstd::expected<int, int>(21 * 2); };
  // auto succeedptr = [](eptr e) { return nonstd::expected<int, eptr>(21*2);};
  auto fail =    [](int) { return nonstd::expected<int, int>(nonstd::unexpect, 17);};
  // auto efail =   [](eptr e) { *e = 17;return nonstd::expected<int, eptr>(nonstd::unexpect, std::move(e));};
  // auto failptr = [](eptr e) { return nonstd::expected<int, eptr>(nonstd::unexpect, std::move(e));};
  auto failvoid = [](int) {};
  // auto failvoidptr = [](const eptr&) { /* don't consume */};
  // auto consumeptr = [](eptr) {};
  // auto make_u_int = [](int n) { return std::unique_ptr<int>(new int(n));};

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

  /*
  {
    nonstd::expected<int, eptr> e = 21;
    auto ret = std::move(e) | utils::orElse(succeedptr);
    REQUIRE(ret);
    REQUIRE(*ret == 21);
  }
   */

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::orElse(succeed);
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


  /*
  {
    nonstd::expected<int, eptr> e = 21;
    auto ret = std::move(e) | utils::orElse(efail);
    REQUIRE(ret);
    REQUIRE(ret == 21);
  }
   */

  {
    const nonstd::expected<int, int> e = 21;
    auto ret = std::move(e) | utils::orElse(fail);
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

  /*
  {
    nonstd::expected<int, eptr> e(nonstd::unexpect, make_u_int(21));
    auto ret = std::move(e) | utils::orElse(succeedptr);
    REQUIRE(ret);
    REQUIRE(*ret == 42);
  }
   */

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(succeed);
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

  /*
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
   */

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(fail);
    REQUIRE(!ret);
    REQUIRE(ret.error() == 17);
  }

  {
    const nonstd::expected<int, int> e(nonstd::unexpect, 21);
    auto ret = std::move(e) | utils::orElse(failvoid);
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
  REQUIRE_THROWS_AS(ex | utils::valueOrElse([](std::string){ throw std::exception(); }), std::exception);
  REQUIRE_THROWS_AS(ex | utils::valueOrElse([](const std::string&) -> int { throw std::exception(); }), std::exception);
  REQUIRE_THROWS_AS(std::move(ex) | utils::valueOrElse([](std::string&&) -> int { throw std::exception(); }), std::exception);
}
