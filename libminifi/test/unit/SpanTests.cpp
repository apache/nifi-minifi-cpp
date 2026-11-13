// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <vector>
#include <string>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/span.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("span_to") {
  const auto test_span = std::span("test text", 9);
  const auto string = utils::span_to<std::string>(test_span);
  const auto vector = utils::span_to<std::vector>(test_span);

  REQUIRE(string == "test text");
  REQUIRE('t' == vector[0]);
  REQUIRE(9 == vector.size());
}

TEST_CASE("as_span") {
  const auto test_span = as_bytes(std::span("test text", 9));
  const auto char_span = utils::as_span<const char>(test_span);

  REQUIRE(char_span.size() == test_span.size());
  REQUIRE(char_span[0] == 't');
}
