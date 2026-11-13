// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "unit/TestBase.h"
#include "unit/Catch.h"
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
