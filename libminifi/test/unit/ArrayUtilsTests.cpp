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

#include "../Catch.h"
#include "utils/ArrayUtils.h"

namespace utils = org::apache::nifi::minifi::utils;

TEST_CASE("array_cat() works correctly and is constexpr") {
  static constexpr auto empty = std::array<int, 0>{};
  static constexpr auto one_to_three = std::array{1, 2, 3};
  static constexpr auto four_to_five = utils::array_cat(empty, std::array{4, 5});
  static constexpr auto all = utils::array_cat(one_to_three, empty, four_to_five, empty);
  CHECK(all == std::array{1, 2, 3, 4, 5});
}

TEST_CASE("string_view_to_array() works correctly and is constexpr") {
  static constexpr std::string_view hello = "Hello world!";
  static constexpr auto hello_array = utils::string_view_to_array<hello.size()>(hello);
  CHECK(std::string_view{hello_array.data(), hello_array.size()} == "Hello world!");

  static constexpr auto hello_again = utils::array_to_string_view(hello_array);
  CHECK(hello_again == "Hello world!");
}

TEST_CASE("getKeys() works correctly and is constexpr") {
  static constexpr std::array<std::pair<std::string_view, int>, 3> mapping{{ {"one", 1}, {"two", 2}, {"three", 3} }};
  static constexpr auto keys = utils::getKeys(mapping);
  CHECK(keys == std::array<std::string_view, 3>{"one", "two", "three"});
}

TEST_CASE("at() works correctly and is constexpr") {
  static constexpr std::array<std::pair<int, std::string_view>, 3> mapping{{ {1, "one"}, {2, "two"}, {3, "three"} }};
  static constexpr auto two = utils::at(mapping, 2);
  CHECK(two == "two");

  int one = 1;
  CHECK(utils::at(mapping, one) == "one");  // non-constexpr argument is OK, but the result is not constexpr

  // static constexpr auto four = utils::at(mapping, 4);  // does not compile
  CHECK_THROWS(utils::at(mapping, 4));
}
