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
#include "../../include/core/Property.h"
#include <string>
#include "utils/StringUtils.h"
#include "core/Property.h"
#include "../TestBase.h"

TEST_CASE("Test Boolean Conversion", "[testboolConversion]") {
  bool b;
  REQUIRE(true == org::apache::nifi::minifi::utils::StringUtils::StringToBool("true", b));
  REQUIRE(true == org::apache::nifi::minifi::utils::StringUtils::StringToBool("True", b));
  REQUIRE(true == org::apache::nifi::minifi::utils::StringUtils::StringToBool("TRue", b));
  REQUIRE(true == org::apache::nifi::minifi::utils::StringUtils::StringToBool("tRUE", b));
  REQUIRE(false == org::apache::nifi::minifi::utils::StringUtils::StringToBool("FALSE", b));
  REQUIRE(false == org::apache::nifi::minifi::utils::StringUtils::StringToBool("FALLSEY", b));
  REQUIRE(false == org::apache::nifi::minifi::utils::StringUtils::StringToBool("FaLSE", b));
  REQUIRE(false == org::apache::nifi::minifi::utils::StringUtils::StringToBool("false", b));
}


TEST_CASE("Test Is it Time", "[testTime]") {
  core::TimeUnit unit;
  int64_t max_partition_millis_;

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToTime("1 SEC", max_partition_millis_, unit));
  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToTime("1 sec", max_partition_millis_, unit));

  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToTime("1 s", max_partition_millis_, unit));
  REQUIRE(true == org::apache::nifi::minifi::core::Property::StringToTime("1 S", max_partition_millis_, unit));
}

TEST_CASE("Test Trimmer Right", "[testTrims]") {
  std::string test = "a quick brown fox jumped over the road\t\n";

  REQUIRE(test.c_str()[test.length() - 1] == '\n');
  REQUIRE(test.c_str()[test.length() - 2] == '\t');
  test = org::apache::nifi::minifi::utils::StringUtils::trimRight(test);

  REQUIRE(test.c_str()[test.length() - 1] == 'd');
  REQUIRE(test.c_str()[test.length() - 2] == 'a');

  test = "a quick brown fox jumped over the road\v\t";

  REQUIRE(test.c_str()[test.length() - 1] == '\t');
  REQUIRE(test.c_str()[test.length() - 2] == '\v');

  test = org::apache::nifi::minifi::utils::StringUtils::trimRight(test);

  REQUIRE(test.c_str()[test.length() - 1] == 'd');
  REQUIRE(test.c_str()[test.length() - 2] == 'a');

  test = "a quick brown fox jumped over the road \f";

  REQUIRE(test.c_str()[test.length() - 1] == '\f');
  REQUIRE(test.c_str()[test.length() - 2] == ' ');

  test = org::apache::nifi::minifi::utils::StringUtils::trimRight(test);

  REQUIRE(test.c_str()[test.length() - 1] == 'd');
}

TEST_CASE("Test Trimmer Left", "[testTrims]") {
  std::string test = "\t\na quick brown fox jumped over the road\t\n";

  REQUIRE(test.c_str()[0] == '\t');
  REQUIRE(test.c_str()[1] == '\n');

  test = org::apache::nifi::minifi::utils::StringUtils::trimLeft(test);

  REQUIRE(test.c_str()[0] == 'a');
  REQUIRE(test.c_str()[1] == ' ');

  test = "\v\ta quick brown fox jumped over the road\v\t";

  REQUIRE(test.c_str()[0] == '\v');
  REQUIRE(test.c_str()[1] == '\t');

  test = org::apache::nifi::minifi::utils::StringUtils::trimLeft(test);

  REQUIRE(test.c_str()[0] == 'a');
  REQUIRE(test.c_str()[1] == ' ');

  test = " \fa quick brown fox jumped over the road \f";

  REQUIRE(test.c_str()[0] == ' ');
  REQUIRE(test.c_str()[1] == '\f');

  test = org::apache::nifi::minifi::utils::StringUtils::trimLeft(test);

  REQUIRE(test.c_str()[0] == 'a');
  REQUIRE(test.c_str()[1] == ' ');
}

TEST_CASE("Test int conversion", "[testStringToInt]") {
  using org::apache::nifi::minifi::core::Property;

  uint64_t uint64_var = 0;
  REQUIRE(Property::StringToInt("-2", uint64_var) == false);  // Negative shouldn't be converted to uint

  uint32_t uint32_var = 0;
  REQUIRE(Property::StringToInt("3  GB", uint32_var) == true);  // Test skipping of spaces, too
  uint32_t expected_value = 3u * 1024 * 1024 * 1024;
  REQUIRE(uint32_var == expected_value);

  int32_t int32_var = 0;
  REQUIRE(Property::StringToInt("3GB", int32_var) == false);  // Doesn't fit

  REQUIRE(Property::StringToInt("-1 G", int32_var) == true);
  REQUIRE(int32_var == -1 * 1000 * 1000 * 1000);

  REQUIRE(Property::StringToInt("-1G", uint32_var) == false);  // Negative to uint

  uint64_t huge_number = uint64_t(std::numeric_limits<int64_t>::max()) +1;

  REQUIRE(Property::StringToInt(std::to_string(huge_number), uint64_var) == true);
  REQUIRE(uint64_var == huge_number);
}

