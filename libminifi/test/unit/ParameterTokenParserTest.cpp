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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/ParameterTokenParser.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Empty string has zero parameters") {
  core::ParameterTokenParser parser("");
  REQUIRE(parser.getTokens().empty());
}

TEST_CASE("Parse a single token") {
  core::ParameterTokenParser parser("#{token.1}");
  REQUIRE(parser.getTokens().size() == 1);
  auto& tokens = parser.getTokens();
  CHECK(tokens.at(0).getName() == "token.1");
  CHECK(tokens.at(0).getStart() == 0);
  CHECK(tokens.at(0).getSize() == 10);
}

TEST_CASE("Parse multiple tokens") {
  core::ParameterTokenParser parser("#{token1} #{token-2}");
  REQUIRE(parser.getTokens().size() == 2);
  auto& tokens = parser.getTokens();
  CHECK(tokens.at(0).getName() == "token1");
  CHECK(tokens.at(0).getStart() == 0);
  CHECK(tokens.at(0).getSize() == 9);
  CHECK(tokens.at(1).getName() == "token-2");
  CHECK(tokens.at(1).getStart() == 10);
  CHECK(tokens.at(1).getSize() == 10);
}

TEST_CASE("Parse the same token multiple times") {
  core::ParameterTokenParser parser("#{token1} #{token-2} #{token1}");
  REQUIRE(parser.getTokens().size() == 3);
  auto& tokens = parser.getTokens();
  CHECK(tokens.at(0).getName() == "token1");
  CHECK(tokens.at(0).getStart() == 0);
  CHECK(tokens.at(0).getSize() == 9);
  CHECK(tokens.at(1).getName() == "token-2");
  CHECK(tokens.at(1).getStart() == 10);
  CHECK(tokens.at(1).getSize() == 10);
  CHECK(tokens.at(2).getName() == "token1");
  CHECK(tokens.at(2).getStart() == 21);
  CHECK(tokens.at(2).getSize() == 9);
}

TEST_CASE("Tokens can be escaped") {
  core::ParameterTokenParser parser("## ##{token1} #{token-2} ###{token_3}# ## #not_a_token");
  REQUIRE(parser.getTokens().size() == 2);
  auto& tokens = parser.getTokens();
  CHECK(tokens.at(0).getName() == "token-2");
  CHECK(tokens.at(0).getStart() == 14);
  CHECK(tokens.at(0).getSize() == 10);
  CHECK(tokens.at(1).getName() == "token_3");
  CHECK(tokens.at(1).getStart() == 27);
  CHECK(tokens.at(1).getSize() == 10);
}

TEST_CASE("Unfinished token is not a token") {
  core::ParameterTokenParser parser("this is #{_token_ 1} and #{token-2 not finished");
  REQUIRE(parser.getTokens().size() == 1);
  auto& tokens = parser.getTokens();
  CHECK(tokens.at(0).getName() == "_token_ 1");
  CHECK(tokens.at(0).getStart() == 8);
  CHECK(tokens.at(0).getSize() == 12);
}

TEST_CASE("Test invalid token names") {
  auto create_error_message = [](const std::string& invalid_name){
    return "Invalid token name: '" + invalid_name + "'. Only alpha-numeric characters (a-z, A-Z, 0-9), hyphens ( - ), underscores ( _ ), periods ( . ), and spaces are allowed in token name.";
  };
  CHECK_THROWS_WITH(core::ParameterTokenParser("#{}"), create_error_message(""));
  CHECK_THROWS_WITH(core::ParameterTokenParser("#{#}"), create_error_message("#"));
  CHECK_THROWS_WITH(core::ParameterTokenParser("#{[]}"), create_error_message("[]"));
  CHECK_THROWS_WITH(core::ParameterTokenParser("#{a{}"), create_error_message("a{"));
  CHECK_THROWS_WITH(core::ParameterTokenParser("#{$$}"), create_error_message("$$"));
}

TEST_CASE("Test token replacement") {
  core::ParameterTokenParser parser("What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more");
  core::ParameterContext context("test_context");
  context.addParameter(core::Parameter{"what", "", "love"});
  context.addParameter(core::Parameter{"who", "", "me"});
  REQUIRE(parser.replaceParameters(context) == "What is love, baby don't hurt me, don't hurt me, no more");
}

TEST_CASE("Test replacement with missing token in context") {
  core::ParameterTokenParser parser("What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more");
  core::ParameterContext context("test_context");
  context.addParameter(core::Parameter{"what", "", "love"});
  REQUIRE_THROWS_WITH(parser.replaceParameters(context), "Parameter 'who' not found");
}

}  // namespace org::apache::nifi::minifi::test
