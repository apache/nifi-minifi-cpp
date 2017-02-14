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
#ifndef LIBMINIFI_TEST_UNIT_PROPERTYTESTS_H_
#define LIBMINIFI_TEST_UNIT_PROPERTYTESTS_H_

#include "utils/StringUtils.h"
#include "../TestBase.h"
#include "Property.h"

TEST_CASE("Test Boolean Conversion", "[testboolConversion]") {

	bool b;
	REQUIRE(true == StringUtils::StringToBool("true",b));
	REQUIRE(true == StringUtils::StringToBool("True",b));
	REQUIRE(true == StringUtils::StringToBool("TRue",b));
	REQUIRE(true == StringUtils::StringToBool("tRUE",b));

	REQUIRE(false == StringUtils::StringToBool("FALSE",b));
	REQUIRE(false == StringUtils::StringToBool("FALLSEY",b));
	REQUIRE(false == StringUtils::StringToBool("FaLSE",b));
	REQUIRE(false == StringUtils::StringToBool("false",b));

}

TEST_CASE("Test Trimmer Right", "[testTrims]") {

	std::string test = "a quick brown fox jumped over the road\t\n";

	REQUIRE(test.c_str()[test.length() - 1] == '\n');
	REQUIRE(test.c_str()[test.length() - 2] == '\t');
	test = StringUtils::trimRight(test);

	REQUIRE(test.c_str()[test.length() - 1] == 'd');
	REQUIRE(test.c_str()[test.length() - 2] == 'a');

	test = "a quick brown fox jumped over the road\v\t";

	REQUIRE(test.c_str()[test.length() - 1] == '\t');
	REQUIRE(test.c_str()[test.length() - 2] == '\v');

	test = StringUtils::trimRight(test);

	REQUIRE(test.c_str()[test.length() - 1] == 'd');
	REQUIRE(test.c_str()[test.length() - 2] == 'a');

	test = "a quick brown fox jumped over the road \f";

	REQUIRE(test.c_str()[test.length() - 1] == '\f');
	REQUIRE(test.c_str()[test.length() - 2] == ' ');

	test = StringUtils::trimRight(test);

	REQUIRE(test.c_str()[test.length() - 1] == 'd');

}

TEST_CASE("Test Trimmer Left", "[testTrims]") {

	std::string test = "\t\na quick brown fox jumped over the road\t\n";

	REQUIRE(test.c_str()[0] == '\t');
	REQUIRE(test.c_str()[1] == '\n');

	test = StringUtils::trimLeft(test);

	REQUIRE(test.c_str()[0] == 'a');
	REQUIRE(test.c_str()[1] == ' ');

	test = "\v\ta quick brown fox jumped over the road\v\t";

	REQUIRE(test.c_str()[0] == '\v');
	REQUIRE(test.c_str()[1] == '\t');

	test = StringUtils::trimLeft(test);

	REQUIRE(test.c_str()[0] == 'a');
	REQUIRE(test.c_str()[1] == ' ');

	test = " \fa quick brown fox jumped over the road \f";

	REQUIRE(test.c_str()[0] == ' ');
	REQUIRE(test.c_str()[1] == '\f');

	test = StringUtils::trimLeft(test);

	REQUIRE(test.c_str()[0] == 'a');
	REQUIRE(test.c_str()[1] == ' ');

}

#endif /* LIBMINIFI_TEST_UNIT_PROPERTYTESTS_H_ */
