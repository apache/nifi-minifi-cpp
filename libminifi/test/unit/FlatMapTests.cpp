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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "minifi-cpp/utils/FlatMap.h"


TEST_CASE("FlatMap operator[]", "[flatmap::subscript]") {
  utils::FlatMap<std::string, std::string> map;
  map.insert(std::make_pair("valid_key", "value"));
  CHECK(map.contains("valid_key"));
  CHECK_FALSE(map.contains("invalid_key"));
  CHECK(map["valid_key"] == "value");
  CHECK(map["invalid_key"].empty());
  CHECK(map.contains("valid_key"));
  CHECK(map.contains("invalid_key"));
}

TEST_CASE("FlatMap at", "[flatmap::at]") {
  utils::FlatMap<std::string, std::string> map;
  map.insert(std::make_pair("valid_key", "value"));
  CHECK(map.contains("valid_key"));
  CHECK_FALSE(map.contains("invalid_key"));
  CHECK(map.at("valid_key") == "value");
  REQUIRE_THROWS_AS(map.at("invalid_key"), std::out_of_range);
  CHECK(map.contains("valid_key"));
  CHECK_FALSE(map.contains("invalid_key"));
}

TEST_CASE("FlatMap const at", "[flatmap::at]") {
  utils::FlatMap<std::string, std::string> map;
  map.insert(std::make_pair("valid_key", "value"));
  const auto& const_map = map;
  CHECK(const_map.contains("valid_key"));
  CHECK_FALSE(const_map.contains("invalid_key"));
  CHECK(const_map.at("valid_key") == "value");
  REQUIRE_THROWS_AS(const_map.at("invalid_key"), std::out_of_range);
  CHECK(const_map.contains("valid_key"));
  CHECK_FALSE(const_map.contains("invalid_key"));
}

TEST_CASE("FlatMap supports equality based lookups") {
  utils::FlatMap<std::string, std::string> map;
  map.insert(std::make_pair("alpha", "value"));
  const std::string string_key = "alpha";
  constexpr std::string_view string_view_key = "alpha";
  constexpr std::string_view invalid_string_view_key = "beta";
  CHECK(map.contains(string_key));
  CHECK(map.contains(string_view_key));
  CHECK_FALSE(map.contains(invalid_string_view_key));

  auto homogeneous_lookup_result = map.find(string_key);
  CHECK(homogeneous_lookup_result != map.end());
  CHECK(map.find(string_view_key) == homogeneous_lookup_result);
  CHECK(map.find(invalid_string_view_key) != homogeneous_lookup_result);
}
