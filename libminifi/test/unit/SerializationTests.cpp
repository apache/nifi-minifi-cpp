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
#include <algorithm>
#include <memory>
#include <string>

#include "SiteToSiteHelper.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "../unit/SiteToSiteHelper.h"

#define FMT_DEFAULT fmt_lower

TEST_CASE("TestWriteUTF", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "helo world";  // yes, this has a typo.
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, false);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("TestWriteUTF2", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "hel\xa1o world";
  REQUIRE(11 == stringOne.length());
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, false);

  REQUIRE(verifyString == stringOne);
}

TEST_CASE("TestWriteUTF3", "[MINIFI193]") {
  org::apache::nifi::minifi::io::BufferStream baseStream;

  std::string stringOne = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";
  REQUIRE(12 == stringOne.length());
  std::string verifyString;
  baseStream.write(stringOne, false);

  baseStream.read(verifyString, false);

  REQUIRE(verifyString == stringOne);
}
