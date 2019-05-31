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
#include <cstdlib>
#include <iostream>
#include "../TestBase.h"
#include "core/Core.h"
#include "utils/file/FileUtils.h"

using org::apache::nifi::minifi::utils::file::FileUtils;

TEST_CASE("TestFileUtils::concat_path", "[TestConcatPath]") {
  std::string child = "baz";
#ifdef WIN32
  std::string base = "foo\\bar";
  REQUIRE("foo\\bar\\baz" == FileUtils::concat_path(base, child));
#else
  std::string base = "foo/bar";
  REQUIRE("foo/bar/baz" == FileUtils::concat_path(base, child));
#endif
}

TEST_CASE("TestFileUtils::get_parent_path", "[TestGetParentPath]") {
#ifdef WIN32
  REQUIRE("foo\\" == FileUtils::get_parent_path("foo\\bar"));
  REQUIRE("foo\\" == FileUtils::get_parent_path("foo\\bar\\"));
  REQUIRE("C:\\foo\\" == FileUtils::get_parent_path("C:\\foo\\bar"));
  REQUIRE("C:\\foo\\" == FileUtils::get_parent_path("C:\\foo\\bar\\"));
  REQUIRE("C:\\" == FileUtils::get_parent_path("C:\\foo"));
  REQUIRE("C:\\" == FileUtils::get_parent_path("C:\\foo\\"));
  REQUIRE("" == FileUtils::get_parent_path("C:\\"));
  REQUIRE("" == FileUtils::get_parent_path("C:\\\\"));
#else
  REQUIRE("foo/" == FileUtils::get_parent_path("foo/bar"));
  REQUIRE("foo/" == FileUtils::get_parent_path("foo/bar/"));
  REQUIRE("/foo/" == FileUtils::get_parent_path("/foo/bar"));
  REQUIRE("/foo/" == FileUtils::get_parent_path("/foo/bar/"));
  REQUIRE("/" == FileUtils::get_parent_path("/foo"));
  REQUIRE("/" == FileUtils::get_parent_path("/foo/"));
  REQUIRE("" == FileUtils::get_parent_path("/"));
  REQUIRE("" == FileUtils::get_parent_path("//"));
#endif
}

TEST_CASE("TestFileUtils::get_executable_path", "[TestGetExecutablePath]") {
  std::string executable_path = FileUtils::get_executable_path();
  std::cerr << "Executable path: " << executable_path << std::endl;
  REQUIRE(0U < executable_path.size());
}

TEST_CASE("TestFileUtils::get_executable_dir", "[TestGetExecutableDir]") {
  std::string executable_path = FileUtils::get_executable_path();
  std::string executable_dir = FileUtils::get_executable_dir();
  REQUIRE(0U < executable_dir.size());
  std::cerr << "Executable dir: " << executable_dir << std::endl;
  REQUIRE(FileUtils::get_parent_path(executable_path) == executable_dir);
}
