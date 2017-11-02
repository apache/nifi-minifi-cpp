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

