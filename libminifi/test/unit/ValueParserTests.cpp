// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <cstdint>

#include "utils/ValueParser.h"
#include "unit/Catch.h"

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
