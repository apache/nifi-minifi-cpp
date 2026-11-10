// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <algorithm>
#include <memory>
#include <string>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SiteToSiteHelper.h"

#define FMT_DEFAULT fmt_lower

TEST_CASE("TestWriteUTF", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "helo world";  // yes, this has a typo.
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, minifi::io::LengthPrefixSize::_16BIT, 100);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("TestWriteUTF2", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "hel\xa1o world";
  REQUIRE(11 == stringOne.length());
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, minifi::io::LengthPrefixSize::_16BIT, 100);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("TestWriteUTF3", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";
  REQUIRE(12 == stringOne.length());
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, minifi::io::LengthPrefixSize::_16BIT, 100);

  REQUIRE(verifyString == stringOne);
}
