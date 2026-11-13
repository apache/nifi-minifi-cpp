// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <map>
#include <unordered_map>
#include <set>
#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/MapUtils.h"

namespace MapUtils = org::apache::nifi::minifi::utils::MapUtils;

TEST_CASE("TestMapUtils::getKeys", "[getKeys]") {
  REQUIRE(MapUtils::getKeys(std::map<int, int>()).empty());
  REQUIRE(MapUtils::getKeys(std::unordered_map<int, int>()).empty());

  std::map<int, std::string> test_map{
    std::make_pair(1, "one"),
    std::make_pair(2, "two"),
    std::make_pair(3, "three"),
  };

  std::unordered_map<int, std::string> test_unordered_map{
    std::make_pair(1, "one"),
    std::make_pair(2, "two"),
    std::make_pair(3, "three"),
  };
  std::set<int> expected_set{1, 2, 3};
  REQUIRE(MapUtils::getKeys(test_map) == expected_set);
  REQUIRE(MapUtils::getKeys(test_unordered_map) == expected_set);
}
