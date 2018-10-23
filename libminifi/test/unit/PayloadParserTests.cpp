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
#include "c2/C2Payload.h"
#include "c2/PayloadParser.h"
#include "../TestBase.h"

TEST_CASE("Test Valid Payload", "[tv1]") {
  std::string ident = "identifier";
  std::string cheese = "cheese";
  std::string chips = "chips";
  minifi::c2::C2Payload payload(minifi::c2::Operation::ACKNOWLEDGE, ident);
  minifi::c2::C2Payload payload2(minifi::c2::Operation::ACKNOWLEDGE, minifi::state::UpdateState::FULLY_APPLIED, cheese);
  minifi::c2::C2ContentResponse response(minifi::c2::Operation::ACKNOWLEDGE);
  response.operation_arguments["type"] = "munster";
  payload2.addContent(std::move(response));
  payload.addPayload(std::move(payload2));
  payload.addPayload(minifi::c2::C2Payload(minifi::c2::Operation::ACKNOWLEDGE, chips));
  REQUIRE("munster" == minifi::c2::PayloadParser::getInstance(payload).in("cheese").getAs<std::string>("type"));
}

TEST_CASE("Test Invalid not found", "[tv2]") {
  std::string ident = "identifier";
  std::string cheese = "cheese";
  std::string chips = "chips";
  minifi::c2::C2Payload payload(minifi::c2::Operation::ACKNOWLEDGE, ident);
  minifi::c2::C2Payload payload2(minifi::c2::Operation::ACKNOWLEDGE, cheese);
  minifi::c2::C2ContentResponse response(minifi::c2::Operation::ACKNOWLEDGE);
  response.operation_arguments["typeS"] = "munster";
  payload2.addContent(std::move(response));
  payload.addPayload(std::move(payload2));
  payload.addPayload(minifi::c2::C2Payload(minifi::c2::Operation::ACKNOWLEDGE, chips));
  REQUIRE_THROWS_AS(minifi::c2::PayloadParser::getInstance(payload).in("cheese").getAs<std::string>("type"), minifi::c2::PayloadParseException);
}


TEST_CASE("Test Invalid coercion", "[tv3]") {
  std::string ident = "identifier";
  std::string cheese = "cheese";
  std::string chips = "chips";
  minifi::c2::C2Payload payload(minifi::c2::Operation::ACKNOWLEDGE, ident);
  minifi::c2::C2Payload payload2(minifi::c2::Operation::ACKNOWLEDGE, minifi::state::UpdateState::FULLY_APPLIED, cheese);
  minifi::c2::C2ContentResponse response(minifi::c2::Operation::ACKNOWLEDGE);
  response.operation_arguments["type"] = "munster";
  payload2.addContent(std::move(response));
  payload.addPayload(std::move(payload2));
  payload.addPayload(minifi::c2::C2Payload(minifi::c2::Operation::ACKNOWLEDGE, chips));
  REQUIRE_THROWS_AS(minifi::c2::PayloadParser::getInstance(payload).in("cheese").getAs<uint64_t>("type"), minifi::c2::PayloadParseException);
}

TEST_CASE("Test Invalid not there", "[tv4]") {
  std::string ident = "identifier";
  std::string cheese = "cheese";
  std::string chips = "chips";
  minifi::c2::C2Payload payload(minifi::c2::Operation::ACKNOWLEDGE, ident);
  minifi::c2::C2Payload payload2(minifi::c2::Operation::ACKNOWLEDGE, minifi::state::UpdateState::FULLY_APPLIED, cheese);
  minifi::c2::C2ContentResponse response(minifi::c2::Operation::ACKNOWLEDGE);
  response.operation_arguments["type"] = "munster";
  payload2.addContent(std::move(response));
  payload.addPayload(std::move(payload2));
  payload.addPayload(minifi::c2::C2Payload(minifi::c2::Operation::ACKNOWLEDGE, chips));
  REQUIRE_THROWS_AS(minifi::c2::PayloadParser::getInstance(payload).in("cheeses").getAs<uint64_t>("type"), minifi::c2::PayloadParseException);
}

TEST_CASE("Test typed conversions", "[tv5]") {
  std::string ident = "identifier";
  std::string cheese = "cheese";
  std::string chips = "chips";
  uint64_t size = 233;
  bool isvalid = false;
  minifi::c2::C2Payload payload(minifi::c2::Operation::ACKNOWLEDGE, ident);
  minifi::c2::C2Payload payload2(minifi::c2::Operation::ACKNOWLEDGE, minifi::state::UpdateState::FULLY_APPLIED, cheese);
  minifi::c2::C2ContentResponse response(minifi::c2::Operation::ACKNOWLEDGE);
  response.operation_arguments["type"] = "munster";
  response.operation_arguments["isvalid"] = isvalid;
  response.operation_arguments["size"] = size;
  payload2.addContent(std::move(response));
  payload.addPayload(std::move(payload2));
  payload.addPayload(minifi::c2::C2Payload(minifi::c2::Operation::ACKNOWLEDGE, chips));
  REQUIRE("munster" == minifi::c2::PayloadParser::getInstance(payload).in("cheese").getAs<std::string>("type"));
  REQUIRE(233 == minifi::c2::PayloadParser::getInstance(payload).in("cheese").getAs<uint64_t>("size"));
  REQUIRE(false == minifi::c2::PayloadParser::getInstance(payload).in("cheese").getAs<bool>("isvalid"));
}


TEST_CASE("Test Invalid not there deep", "[tv6]") {
  std::string ident = "identifier";
  std::string cheese = "cheese";
  std::string chips = "chips";
  minifi::c2::C2Payload payload(minifi::c2::Operation::ACKNOWLEDGE, ident);
  minifi::c2::C2Payload payload2(minifi::c2::Operation::ACKNOWLEDGE, minifi::state::UpdateState::FULLY_APPLIED, cheese);
  minifi::c2::C2ContentResponse response(minifi::c2::Operation::ACKNOWLEDGE);
  response.operation_arguments["type"] = "munster";
  payload2.addContent(std::move(response));
  payload.addPayload(std::move(payload2));
  payload.addPayload(minifi::c2::C2Payload(minifi::c2::Operation::ACKNOWLEDGE, chips));
  REQUIRE_THROWS_AS(minifi::c2::PayloadParser::getInstance(payload).in("chips").getAs<uint64_t>("type"), minifi::c2::PayloadParseException);
}
