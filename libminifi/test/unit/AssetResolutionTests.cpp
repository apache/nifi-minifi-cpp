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
#include "core/ReferenceParser.h"

namespace org::apache::nifi::minifi::test {

static std::string resolve(const std::string& str) {
  return core::resolveIdentifier(str, {core::getAssetResolver([] (const std::string& id) -> std::optional<std::filesystem::path> {
    if (id == "apple") {
      return "/home/user/apple.txt";
    }
    if (id == "banana") {
      return "/home/user/banana.txt";
    }
    if (id == "471deef6-2a6e-4a7d-912a-81cc17e3a204") {
      return "/home/ai_model.txt";
    }
    return std::nullopt;
  })});
}

TEST_CASE("Non-references are preserved") {
  const std::string str = "property #{this} other {{nothing}} else";
  const auto result = resolve(str);
  REQUIRE(result == str);
}

TEST_CASE("Can escape '@'") {
  REQUIRE(resolve("property @@{this} other {{nothing}} else@@") == "property @{this} other {{nothing}} else@");
}

TEST_CASE("Throw on unterminated pattern") {
  REQUIRE_THROWS_AS(resolve("something @"), core::MalformedReferenceException);
  REQUIRE_THROWS_AS(resolve("something @{"), core::MalformedReferenceException);
  REQUIRE_THROWS_AS(resolve("something @{}"), core::MalformedReferenceException);
  REQUIRE_THROWS_AS(resolve("something @{:"), core::MalformedReferenceException);
}

TEST_CASE("Resolve existing asset") {
  REQUIRE(resolve("something @{asset-id:apple} @{asset-id:banana}") == "something /home/user/apple.txt /home/user/banana.txt");
  REQUIRE(resolve("@{asset-id:apple}") == "/home/user/apple.txt");
  REQUIRE(resolve("@{asset-id:471deef6-2a6e-4a7d-912a-81cc17e3a204}") == "/home/ai_model.txt");
}

TEST_CASE("Throw on non-existing asset") {
  REQUIRE_THROWS_AS(resolve("something @{asset-id:kiwi}"), core::AssetException);
}

TEST_CASE("Throw on non-existing category") {
  REQUIRE_THROWS_AS(resolve("something @{drink:cider}"), core::UnknownCategoryException);
}

TEST_CASE("Resolve more categories") {
  auto result = core::resolveIdentifier("@{drink:cider} @{asset-id:apple}", {core::getAssetResolver([] (const std::string& id) -> std::optional<std::filesystem::path> {
    if (id == "apple") {
      return "/home/user/apple.txt";
    }
    return std::nullopt;
  }), [] (const std::string& category, const std::string& id) -> std::optional<std::string> {
    if (category != "drink") {
      return std::nullopt;
    }
    if (id == "cider") {
      return "yes please";
    }
    return std::nullopt;
  }});
  REQUIRE(result == "yes please /home/user/apple.txt");
}

}  // namespace org::apache::nifi::minifi::test
