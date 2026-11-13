// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <string>
#include "unit/Catch.h"
#include "utils/file/PathUtils.h"

namespace fs = org::apache::nifi::minifi::utils::file;

TEST_CASE("file::globToRegex works", "[globToRegex]") {
  REQUIRE(fs::globToRegex("").empty());
  REQUIRE(fs::globToRegex("NoSpecialChars") == "NoSpecialChars");
  REQUIRE(fs::globToRegex("ReplaceDot.txt") == "ReplaceDot\\.txt");
  REQUIRE(fs::globToRegex("Replace.Multiple.Dots...txt") == "Replace\\.Multiple\\.Dots\\.\\.\\.txt");
  REQUIRE(fs::globToRegex("ReplaceAsterisk.*") == "ReplaceAsterisk\\..*");
  REQUIRE(fs::globToRegex("Replace*Multiple*Asterisks") == "Replace.*Multiple.*Asterisks");
  REQUIRE(fs::globToRegex("ReplaceQuestionMark?.txt") == "ReplaceQuestionMark.\\.txt");
}
