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

#include <map>
#include <unordered_map>
#include <set>
#include <string>

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/MapUtils.h"

namespace MapUtils = org::apache::nifi::minifi::utils::MapUtils;

TEST_CASE("TestMapUtils::getKeys", "[getKeys]") {
  REQUIRE(MapUtils::getKeys(std::map<int, int>()) == std::set<int>());
  REQUIRE(MapUtils::getKeys(std::unordered_map<int, int>()) == std::set<int>());

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
