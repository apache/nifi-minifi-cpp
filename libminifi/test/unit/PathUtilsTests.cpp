/**
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
#include "../Catch.h"
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

TEST_CASE("path::isAbsolutePath", "[path::isAbsolutePath]") {
#ifdef WIN32
  REQUIRE(fs::isAbsolutePath("C:\\"));
  REQUIRE(fs::isAbsolutePath("C:\\Program Files"));
  REQUIRE(fs::isAbsolutePath("C:\\Program Files\\ApacheNiFiMiNiFi\\nifi-minifi-cpp\\conf\\minifi.properties"));
  REQUIRE(fs::isAbsolutePath("C:/"));
  REQUIRE(fs::isAbsolutePath("C:/Program Files"));
  REQUIRE(fs::isAbsolutePath("C:/Program Files/ApacheNiFiMiNiFi/nifi-minifi-cpp/conf/minifi.properties"));
  REQUIRE(!fs::isAbsolutePath("/"));
#else
  REQUIRE(fs::isAbsolutePath("/"));
  REQUIRE(fs::isAbsolutePath("/etc"));
  REQUIRE(fs::isAbsolutePath("/opt/minifi/conf/minifi.properties"));
#endif /* WIN32 */

  REQUIRE(!fs::isAbsolutePath("hello"));
}
