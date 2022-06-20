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
#include "../Catch.h"
#include "data/SQLColumnIdentifier.h"

using org::apache::nifi::minifi::sql::SQLColumnIdentifier;

TEST_CASE("Handles escaped identifiers") {
  REQUIRE(SQLColumnIdentifier("Abc").value() == "Abc");
  REQUIRE(SQLColumnIdentifier("\"Abc\"").value() == "Abc");  // standard
  REQUIRE(SQLColumnIdentifier("[Abc]").value() == "Abc");  // MS SQL
  REQUIRE(SQLColumnIdentifier("`Abc`").value() == "Abc");  // MySQL
  REQUIRE(SQLColumnIdentifier("\"").value() == "\"");  // single char is ignored
}

TEST_CASE("Can return the original representation") {
  REQUIRE(SQLColumnIdentifier("Abc").str() == "Abc");
  REQUIRE(SQLColumnIdentifier("\"Abc\"").str() == "\"Abc\"");
  REQUIRE(SQLColumnIdentifier("[Abc]").str() == "[Abc]");
  REQUIRE(SQLColumnIdentifier("`Abc`").str() == "`Abc`");
}

TEST_CASE("Equality is escape-agnostic") {
  REQUIRE(SQLColumnIdentifier("Abc") == SQLColumnIdentifier("\"Abc\""));
  REQUIRE(SQLColumnIdentifier("\"Abc\"") == SQLColumnIdentifier("[Abc]"));
  REQUIRE(SQLColumnIdentifier("[Abc]") == SQLColumnIdentifier("`Abc`"));
  REQUIRE(SQLColumnIdentifier("\"Abc\"") == SQLColumnIdentifier("`Abc`"));
}

TEST_CASE("Hashing is escape-agnostic") {
  std::unordered_set<SQLColumnIdentifier> ids;
  ids.insert(SQLColumnIdentifier("[Abc]"));
  REQUIRE(ids.count(SQLColumnIdentifier("\"Abc\"")) == 1);
  REQUIRE(ids.count(SQLColumnIdentifier("[Abc]")) == 1);
  REQUIRE(ids.count(SQLColumnIdentifier("`Abc`")) == 1);
  REQUIRE(ids.count(SQLColumnIdentifier("Abc")) == 1);
  REQUIRE(!ids.contains(SQLColumnIdentifier("abc")));
}
