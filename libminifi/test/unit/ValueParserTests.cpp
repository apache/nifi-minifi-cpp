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

#include <cstdint>

#include "utils/ValueParser.h"
#include "../Catch.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("toNumber() can parse valid numbers") {
  CHECK(utils::toNumber<bool>("false") == std::optional<bool>(false));
  CHECK(utils::toNumber<bool>("true") == std::optional<bool>(true));
  CHECK(utils::toNumber<int32_t>("-56483") == std::optional<int32_t>(-56483));
  CHECK(utils::toNumber<uint32_t>("8431") == std::optional<uint32_t>(8431));
  CHECK(utils::toNumber<int64_t>("4781362456") == std::optional<int64_t>(4781362456));
  CHECK(utils::toNumber<uint64_t>("56447216560") == std::optional<uint64_t>(56447216560));
  CHECK(utils::toNumber<double>("-4.1248457") == std::optional<double>(-4.1248457));
}

TEST_CASE("toNumber() returns nullopt if the argument is not a valid number") {
  CHECK(utils::toNumber<bool>("") == std::nullopt);
  CHECK(utils::toNumber<bool>("maybe") == std::nullopt);
  CHECK(utils::toNumber<int32_t>("999000999000") == std::nullopt);
  CHECK(utils::toNumber<uint32_t>("-561") == std::nullopt);
  CHECK(utils::toNumber<int64_t>("0.5") == std::nullopt);
  CHECK(utils::toNumber<uint64_t>("MAXINT") == std::nullopt);
  CHECK(utils::toNumber<double>("sqrt(-1)") == std::nullopt);
}
