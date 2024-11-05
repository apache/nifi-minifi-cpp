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
#include "utils/crypto/property_encryption/PropertyEncryptionUtils.h"

namespace org::apache::nifi::minifi::test {

TEST_CASE("Empty string has zero parameters") {
  core::NonSensitiveParameterTokenParser parser("");
  REQUIRE(parser.getTokens().empty());
}

TEST_CASE("Parse a single token") {
  core::NonSensitiveParameterTokenParser parser("#{token.1}");
  auto& tokens = parser.getTokens();
  REQUIRE(tokens.size() == 1);
  CHECK(tokens.at(0)->getName().value() == "token.1");
  CHECK(tokens.at(0)->getStart() == 0);
  CHECK(tokens.at(0)->getSize() == 10);
}

TEST_CASE("Parse multiple tokens") {
  core::NonSensitiveParameterTokenParser parser("#{token1} #{token-2}");
  auto& tokens = parser.getTokens();
  REQUIRE(tokens.size() == 2);
  CHECK(tokens.at(0)->getName().value() == "token1");
  CHECK(tokens.at(0)->getStart() == 0);
  CHECK(tokens.at(0)->getSize() == 9);
  CHECK(tokens.at(1)->getName().value() == "token-2");
  CHECK(tokens.at(1)->getStart() == 10);
  CHECK(tokens.at(1)->getSize() == 10);
}

TEST_CASE("Parse the same token multiple times") {
  core::NonSensitiveParameterTokenParser parser("#{token1} #{token-2} #{token1}");
  auto& tokens = parser.getTokens();
  REQUIRE(tokens.size() == 3);
  CHECK(tokens.at(0)->getName().value() == "token1");
  CHECK(tokens.at(0)->getStart() == 0);
  CHECK(tokens.at(0)->getSize() == 9);
  CHECK(tokens.at(1)->getName().value() == "token-2");
  CHECK(tokens.at(1)->getStart() == 10);
  CHECK(tokens.at(1)->getSize() == 10);
  CHECK(tokens.at(2)->getName().value() == "token1");
  CHECK(tokens.at(2)->getStart() == 21);
  CHECK(tokens.at(2)->getSize() == 9);
}

TEST_CASE("Tokens can be escaped") {
  core::NonSensitiveParameterTokenParser parser("## ##{token1} #{token-2} ###{token_3}# ## ##not_a_token");
  auto& tokens = parser.getTokens();
  REQUIRE(tokens.size() == 3);
  CHECK(tokens.at(0)->getValue().value() == "#{token1}");
  CHECK(tokens.at(0)->getStart() == 3);
  CHECK(tokens.at(0)->getSize() == 10);
  CHECK(tokens.at(1)->getName().value() == "token-2");
  CHECK(tokens.at(1)->getStart() == 14);
  CHECK(tokens.at(1)->getSize() == 10);
  CHECK(tokens.at(2)->getName().value() == "token_3");
  CHECK(tokens.at(2)->getStart() == 25);
  CHECK(tokens.at(2)->getSize() == 12);
}

TEST_CASE("Unfinished token is not a token") {
  core::NonSensitiveParameterTokenParser parser("this is #{_token_ 1} and #{token-2 not finished");
  auto& tokens = parser.getTokens();
  REQUIRE(tokens.size() == 1);
  CHECK(tokens.at(0)->getName().value() == "_token_ 1");
  CHECK(tokens.at(0)->getStart() == 8);
  CHECK(tokens.at(0)->getSize() == 12);
}

TEST_CASE("Test invalid token names") {
  auto create_error_message = [](const std::string& invalid_name){
    return "Parameter Operation: Invalid token name: '" + invalid_name +
      "'. Only alpha-numeric characters (a-z, A-Z, 0-9), hyphens ( - ), underscores ( _ ), periods ( . ), and spaces are allowed in token name.";
  };
  CHECK_THROWS_WITH(core::NonSensitiveParameterTokenParser("#{}"), create_error_message(""));
  CHECK_THROWS_WITH(core::NonSensitiveParameterTokenParser("#{#}"), create_error_message("#"));
  CHECK_THROWS_WITH(core::NonSensitiveParameterTokenParser("#{[]}"), create_error_message("[]"));
  CHECK_THROWS_WITH(core::NonSensitiveParameterTokenParser("#{a{}"), create_error_message("a{"));
  CHECK_THROWS_WITH(core::NonSensitiveParameterTokenParser("#{$$}"), create_error_message("$$"));
}

TEST_CASE("Test token replacement") {
  core::NonSensitiveParameterTokenParser parser("## What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more ##");
  core::ParameterContext context("test_context");
  context.addParameter(core::Parameter{"what", "", false, false, "love"});
  context.addParameter(core::Parameter{"who", "", false, false, "me"});
  REQUIRE(parser.replaceParameters(&context) == "## What is love, baby don't hurt me, don't hurt me, no more ##");
}

TEST_CASE("Test replacement with escaped tokens") {
  core::NonSensitiveParameterTokenParser parser("### What is #####{what}, baby don't hurt ###{who}, don't hurt ###{who}, no ####{more} ##{");
  REQUIRE(parser.getTokens().size() == 4);
  core::ParameterContext context("test_context");
  context.addParameter(core::Parameter{"what", "", false, false, "love"});
  context.addParameter(core::Parameter{"who", "", false, false, "me"});
  REQUIRE(parser.replaceParameters(&context) == "### What is ##love, baby don't hurt #me, don't hurt #me, no ##{more} ##{");
}

TEST_CASE("Test replacement with missing token in context") {
  core::NonSensitiveParameterTokenParser parser("What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more");
  core::ParameterContext context("test_context");
  context.addParameter(core::Parameter{"what", "", false, false, "love"});
  REQUIRE_THROWS_WITH(parser.replaceParameters(&context), "Parameter Operation: Parameter 'who' not found");
}

TEST_CASE("Sensitive property parameter replacement is not supported") {
  utils::crypto::Bytes secret_key = utils::string::from_hex("cb76fe6fe4cbfdc3770c0cb0afc910f81ced4d436b11f691395fc2a9dbea27ca");
  utils::crypto::EncryptionProvider encryption_provider{secret_key};
  core::SensitiveParameterTokenParser parser("What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more", encryption_provider);
  core::ParameterContext context("test_context");
  context.addParameter(core::Parameter{"what", "", false, false, "love"});
  context.addParameter(core::Parameter{"who", "", false, false, "me"});
  REQUIRE_THROWS_WITH(parser.replaceParameters(&context), "Parameter Operation: Non-sensitive parameter 'what' cannot be referenced in a sensitive property");
}

TEST_CASE("Parameter context is not provided when parameter is referenced") {
  core::NonSensitiveParameterTokenParser parser("What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more");
  REQUIRE_THROWS_WITH(parser.replaceParameters(nullptr), "Parameter Operation: Property references a parameter in its value, but no parameter context was provided.");
}

TEST_CASE("Replace only escaped tokens") {
  core::NonSensitiveParameterTokenParser non_sensitive_parser("No ##{parameters} are ####{present}");
  REQUIRE(non_sensitive_parser.replaceParameters(nullptr) == "No #{parameters} are ##{present}");
  utils::crypto::Bytes secret_key = utils::string::from_hex("cb76fe6fe4cbfdc3770c0cb0afc910f81ced4d436b11f691395fc2a9dbea27ca");
  utils::crypto::EncryptionProvider encryption_provider{secret_key};
  core::SensitiveParameterTokenParser sensitive_parser("No ##{parameters} are ####{present}", encryption_provider);
  REQUIRE(sensitive_parser.replaceParameters(nullptr) == "No #{parameters} are ##{present}");
}

TEST_CASE("Test sensitive token replacement") {
  core::ParameterContext context("test_context");
  utils::crypto::Bytes secret_key = utils::string::from_hex("cb76fe6fe4cbfdc3770c0cb0afc910f81ced4d436b11f691395fc2a9dbea27ca");
  utils::crypto::EncryptionProvider encryption_provider{secret_key};
  core::SensitiveParameterTokenParser parser("What is #{what}, baby don't hurt #{who}, don't hurt #{who}, no more", encryption_provider);
  auto value1 = utils::crypto::property_encryption::encrypt("love", encryption_provider);
  auto value2 = utils::crypto::property_encryption::encrypt("me", encryption_provider);
  context.addParameter(core::Parameter{"what", "", true, false, value1});
  context.addParameter(core::Parameter{"who", "", true, false, value2});
  REQUIRE(parser.replaceParameters(&context) == "What is love, baby don't hurt me, don't hurt me, no more");
}

}  // namespace org::apache::nifi::minifi::test
