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

#include "utils/RegexUtils.h"
#include "../TestBase.h"
#include "../Catch.h"

using org::apache::nifi::minifi::utils::Regex;
namespace minifi = org::apache::nifi::minifi;

TEST_CASE("TestRegexUtils::single_match", "[regex1]") {
  std::string pat = "Speed limit 130 | Speed limit 80";
  std::string rgx1 = "Speed limit ([0-9]+)";
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r1(rgx1, mode);
  REQUIRE(minifi::utils::regexSearch(pat, r1));
}

TEST_CASE("TestRegexUtils::invalid_construction", "[regex2]") {
  std::string pat = "Speed limit 130 | Speed limit 80";
  std::string rgx1 = "Speed limit ([0-9]+)";
  std::string rgx2 = "[Invalid)A(F)";
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r1(rgx1, mode);
  REQUIRE_THROWS_WITH(Regex(rgx2, mode), Catch::Contains("Regex Operation"));
}

TEST_CASE("TestRegexUtils::empty_input", "[regex3]") {
  std::string pat = "";
  std::string rgx1 = "Speed limit ([0-9]+)";
  std::string rgx2 = "";
  std::string rgx3 = "(.*)";
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r1(rgx1, mode);
  REQUIRE(!minifi::utils::regexSearch(pat, r1));
  Regex r2(rgx2, mode);
  REQUIRE(minifi::utils::regexSearch(pat, r2));
  REQUIRE(!minifi::utils::regexSearch("LMN", r1));
  Regex r3(rgx3);
  REQUIRE(minifi::utils::regexSearch(pat, r3));
}

TEST_CASE("TestRegexUtils::check_mode", "[regex4]") {
  std::string pat = "Speed limit 130 | Speed limit 80";
  std::string rgx1 = "sPeeD limIt ([0-9]+)";
  Regex r1(rgx1);
  REQUIRE(!minifi::utils::regexSearch(pat, r1));
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r2(rgx1, mode);
  REQUIRE(minifi::utils::regexSearch(pat, r2));
}

TEST_CASE("TestRegexUtils::regexMatch works correctly", "[matchesFullInput]") {
  REQUIRE(minifi::utils::regexMatch("", Regex("")) == true);
  REQUIRE(minifi::utils::regexMatch("input", Regex("")) == false);
  REQUIRE(minifi::utils::regexMatch("input", Regex(".*")) == true);
  REQUIRE(minifi::utils::regexMatch("input", Regex("np")) == false);
  REQUIRE(minifi::utils::regexMatch("input", Regex(".*np.*")) == true);
  REQUIRE(minifi::utils::regexMatch("input", Regex("(in|out)put")) == true);
  REQUIRE(minifi::utils::regexMatch("input", Regex("inpu[aeiou]*")) == false);
}

TEST_CASE("TestRegexUtils::regexSearch works with groups", "[matchesFullInput]") {
  std::string pat = "Speed limit 130 | Speed limit 80";
  std::string rgx1 = "Speed limit ([0-9]+)";
  Regex r1(rgx1);
  minifi::utils::SMatch matches;
  REQUIRE(minifi::utils::regexSearch(pat, matches, r1));
  REQUIRE(matches.size() == 2);
  REQUIRE(matches[0].str() == "Speed limit 130");
  REQUIRE(matches[1].str() == "130");
  REQUIRE(" | Speed limit 80" == matches.suffix().str());
}

TEST_CASE("TestRegexUtils::regexMatch works with groups", "[matchesFullInput]") {
  std::string pat = "Speed limit 130 all the way";
  std::string rgx1 = "Speed limit ([0-9]+) (.*)";
  Regex r1(rgx1);
  minifi::utils::SMatch matches;
  REQUIRE(minifi::utils::regexMatch(pat, matches, r1));
  REQUIRE(matches.size() == 3);
  REQUIRE(matches[0].str() == "Speed limit 130 all the way");
  REQUIRE(matches[1].str() == "130");
  REQUIRE(matches[2].str() == "all the way");
  REQUIRE("" == matches.suffix().str());
}

TEST_CASE("TestRegexUtils::getLastRegexMatch works correctly", "[getLastRegexMatch]") {
  utils::Regex pattern("<[0-9]+>");
  {
    std::string content = "Foo";
    auto last_match = minifi::utils::getLastRegexMatch(content, pattern);
    REQUIRE_FALSE(last_match.ready());
  }
  {
    std::string content = "<1> Foo";
    auto last_match = minifi::utils::getLastRegexMatch(content, pattern);
    REQUIRE(last_match.ready());
    CHECK(last_match.length(0) == 3);
    CHECK(last_match.position(0) == 0);
  }
  {
    std::string content = "<1> Foo<2> Bar<3> Baz<10> Qux";
    auto last_match = minifi::utils::getLastRegexMatch(content, pattern);
    REQUIRE(last_match.ready());
    CHECK(last_match.length(0) == 4);
    CHECK(last_match.position(0) == 21);
  }
}
