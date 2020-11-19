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

#include <string>
#include <vector>
#include <tuple>
#include "utils/StringView.h"
#include "utils/StringViewUtils.h"
#include "../TestBase.h"

using utils::StringView;
using utils::StringViewUtils;

TEST_CASE("Comparison") {
  std::string a = "a";
  std::string b = "b";

  REQUIRE(StringView(a) == "a");
  REQUIRE(StringView(a) != "b");
  REQUIRE(StringView("a") == "a");
  REQUIRE(StringView("abcd") != "a");
  REQUIRE(StringView(a) == a);
  REQUIRE(StringView(a) != b);
  REQUIRE(StringView(a) == StringView(a));
  REQUIRE(StringView(a) != StringView(b));
  REQUIRE(!(StringView(a) == StringView(b)));

  REQUIRE("a" == StringView(a));
  REQUIRE("b" != StringView(a));
  REQUIRE("a" == StringView("a"));
  REQUIRE("a" != StringView("abcd"));
  REQUIRE(a == StringView(a));
  REQUIRE(b != StringView(a));
}

TEST_CASE("trimLeft") {
  std::vector<std::pair<std::string, std::string>> cases{
      {"", ""},
      {"abcd", "abcd"},
      {" abc", "abc"},
      {"  abc", "abc"},
      {" ", ""},
      {"abc ", "abc "},
      {"\t \r \nabc\r\v", "abc\r\v"},
      {"\t\r\n\v", ""}
  };
  for (const auto& test_case : cases) {
    REQUIRE(StringViewUtils::trimLeft(StringView(test_case.first)) == StringView(test_case.second));
  }
}

TEST_CASE("trimRight") {
  std::vector<std::pair<std::string, std::string>> cases{
      {"", ""},
      {"abcd", "abcd"},
      {"abc ", "abc"},
      {"abc  ", "abc"},
      {" ", ""},
      {" abc", " abc"},
      {"\tabc\t \r \n", "\tabc"},
      {"\t\r\v\n", ""}
  };
  for (const auto& test_case : cases) {
    REQUIRE(StringViewUtils::trimRight(StringView(test_case.first)) == StringView(test_case.second));
  }
}

TEST_CASE("trim") {
  std::vector<std::pair<std::string, std::string>> cases{
      {"", ""},
      {"abcd", "abcd"},
      {"abc ", "abc"},
      {" abc  ", "abc"},
      {" ", ""},
      {" abc", "abc"},
      {"\n\tabc\t \r \n", "abc"},
      {"\v\t\r\n", ""}
  };
  for (const auto& test_case : cases) {
    REQUIRE(StringViewUtils::trim(StringView(test_case.first)) == StringView(test_case.second));
  }

  REQUIRE(StringViewUtils::trim(StringView(" abc  ")) == "abc");
}

TEST_CASE("equalsIgnoreCase") {
  std::vector<std::pair<std::string, std::string>> cases{
      {"", ""},
      {"Abcd", "abcd"},
      {"aBc", "aBc"},
      {"abc", "abc"},
      {"A", "a"},
      {"YES", "yes"}
  };
  for (const auto& test_case : cases) {
    REQUIRE(StringViewUtils::equalsIgnoreCase(StringView(test_case.first), StringView(test_case.second)));
  }
}

TEST_CASE("toBool") {
  std::vector<std::pair<std::string, utils::optional<bool>>> cases{
      {"", {}},
      {"true", true},
      {"false", false},
      {" TrUe   ", true},
      {"\n \r FaLsE \t", false},
      {"not false", {}}
  };
  for (const auto& test_case : cases) {
    REQUIRE(StringViewUtils::toBool(StringView(test_case.first)) == test_case.second);
  }
}
