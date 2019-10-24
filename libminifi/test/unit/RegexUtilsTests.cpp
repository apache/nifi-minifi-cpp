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

#include "../TestBase.h"
#include "utils/RegexUtils.h"
#include "Exception.h"
#include <string>
#include <vector>

using org::apache::nifi::minifi::utils::Regex;
using org::apache::nifi::minifi::Exception;

TEST_CASE("TestRegexUtils::single_match", "[regex1]") {
    std::string pat = "Speed limit 130 | Speed limit 80";
    std::string rgx1 = "Speed limit ([0-9]+)";
    std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
    Regex r1(rgx1, mode);
    REQUIRE(r1.match(pat));
    auto ret = r1.getResult();
    std::vector<std::string> ans = {"Speed limit 130", "130"};
    REQUIRE(ans == ret);
    REQUIRE(" | Speed limit 80" == r1.getSuffix());
}

TEST_CASE("TestRegexUtils::invalid_construction", "[regex2]") {
  std::string pat = "Speed limit 130 | Speed limit 80";
  std::string rgx1 = "Speed limit ([0-9]+)";
  std::string rgx2 = "[Invalid)A(F)";
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r1(rgx1, mode);
  REQUIRE_THROWS_WITH(Regex r2(rgx2, mode), Catch::Contains("Regex Operation"));
}

TEST_CASE("TestRegexUtils::empty_input", "[regex3]") {
  std::string pat = "";
  std::string rgx1 = "Speed limit ([0-9]+)";
  std::string rgx2 = "";
  std::string rgx3 = "(.*)";
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r1(rgx1, mode);
  REQUIRE(!r1.match(pat));
  Regex r2(rgx2, mode);
  REQUIRE(!r2.match(pat));
  REQUIRE(!r2.match("LMN"));
  Regex r3(rgx3);
  REQUIRE(r3.match(pat));
}

TEST_CASE("TestRegexUtils::check_mode", "[regex4]") {
  std::string pat = "Speed limit 130 | Speed limit 80";
  std::string rgx1 = "sPeeD limIt ([0-9]+)";
  Regex r1(rgx1);
  REQUIRE(!r1.match(pat));
  std::vector<Regex::Mode> mode = {Regex::Mode::ICASE};
  Regex r2(rgx1, mode);
  REQUIRE(r2.match(pat));
}
