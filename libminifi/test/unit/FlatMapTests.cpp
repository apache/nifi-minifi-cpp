// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
