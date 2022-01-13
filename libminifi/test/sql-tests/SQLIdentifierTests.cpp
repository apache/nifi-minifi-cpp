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

#undef NDEBUG

#include "../TestBase.h"
#include "data/SQLIdentifier.h"

using org::apache::nifi::minifi::sql::SQLIdentifier;

TEST_CASE("Handles escaped identifiers") {
  REQUIRE(SQLIdentifier("Abc").value() == "Abc");
  REQUIRE(SQLIdentifier("\"Abc\"").value() == "Abc");  // standard
  REQUIRE(SQLIdentifier("[Abc]").value() == "Abc");  // MS SQL
  REQUIRE(SQLIdentifier("`Abc`").value() == "Abc");  // MySQL
  REQUIRE(SQLIdentifier("\"").value() == "\"");  // single char is ignored
}

TEST_CASE("Can return the original representation") {
  REQUIRE(SQLIdentifier("Abc").str() == "Abc");
  REQUIRE(SQLIdentifier("\"Abc\"").str() == "\"Abc\"");
  REQUIRE(SQLIdentifier("[Abc]").str() == "[Abc]");
  REQUIRE(SQLIdentifier("`Abc`").str() == "`Abc`");
}

TEST_CASE("Equality is escape-agnostic") {
  REQUIRE(SQLIdentifier("Abc") == SQLIdentifier("\"Abc\""));
  REQUIRE(SQLIdentifier("\"Abc\"") == SQLIdentifier("[Abc]"));
  REQUIRE(SQLIdentifier("[Abc]") == SQLIdentifier("`Abc`"));
  REQUIRE(SQLIdentifier("\"Abc\"") == SQLIdentifier("`Abc`"));
}

TEST_CASE("Hashing is escape-agnostic") {
  std::unordered_set<SQLIdentifier> ids;
  ids.insert(SQLIdentifier("[Abc]"));
  REQUIRE(ids.count(SQLIdentifier("\"Abc\"")) == 1);
  REQUIRE(ids.count(SQLIdentifier("[Abc]")) == 1);
  REQUIRE(ids.count(SQLIdentifier("`Abc`")) == 1);
  REQUIRE(ids.count(SQLIdentifier("Abc")) == 1);
  REQUIRE(ids.count(SQLIdentifier("abc")) == 0);
}
