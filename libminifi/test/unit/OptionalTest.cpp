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

TEST_CASE("optional operator| (map)", "[optional map]") {
  const auto test1 = utils::make_optional(6) | [](const int i) { return i * 2; };
  REQUIRE(12 == test1.value());

  const auto test2 = utils::optional<int>{} | [](const int i) { return i * 2; };
  REQUIRE(!test2.has_value());
}

TEST_CASE("optional operator>>= (bind)", "[optional bind]") {
  const auto make_intdiv_noremainder = [](const int denom) {
    return [denom](const int num) { return num % denom == 0 ? utils::make_optional(num / denom) : utils::optional<int>{}; };
  };

  const auto test1 = utils::make_optional(6) >>= make_intdiv_noremainder(3);
  REQUIRE(2 == test1.value());

  const auto test2 = utils::optional<int>{} >>= make_intdiv_noremainder(4);
  REQUIRE(!test2.has_value());

  const auto test3 = utils::make_optional(7) >>= make_intdiv_noremainder(3);
  REQUIRE(!test3.has_value());
}
