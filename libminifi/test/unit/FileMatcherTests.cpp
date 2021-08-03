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

#define CUSTOM_EXTENSION_INIT

#include "../TestBase.h"
#include "../Path.h"
#include "utils/file/FileMatcher.h"

struct FileMatcherTestAccessor {
  using FilePattern = fileutils::FileMatcher::FilePattern;
};

using FilePattern = FileMatcherTestAccessor::FilePattern;
using FileMatcher = fileutils::FileMatcher;

#define REQUIRE_INCLUDE(val) REQUIRE((val == FilePattern::MatchResult::INCLUDE))
#define REQUIRE_EXCLUDE(val) REQUIRE((val == FilePattern::MatchResult::EXCLUDE))
#define REQUIRE_DONT_CARE(val) REQUIRE((val == FilePattern::MatchResult::DONT_CARE))

TEST_CASE("Invalid paths") {
  REQUIRE_FALSE(FilePattern::fromPattern(""));
  REQUIRE_FALSE(FilePattern::fromPattern("."));
  REQUIRE_FALSE(FilePattern::fromPattern(".."));
  REQUIRE_FALSE(FilePattern::fromPattern("!"));
  REQUIRE_FALSE(FilePattern::fromPattern("!."));
  REQUIRE_FALSE(FilePattern::fromPattern("!.."));
  // parent accessor after wildcard
  REQUIRE(FilePattern::fromPattern("./../file.txt"));  // sanity check
  REQUIRE_FALSE(FilePattern::fromPattern("./*/../file.txt"));
}

TEST_CASE("Matching directories without globs") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "banana" / "file").str()).value();
  REQUIRE_INCLUDE(pattern.match((root / "one").str()));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "two").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "apple").str()));
  // this looks at the DIRECTORY "/one/banana/file", while our pattern
  // looks for the FILE "/one/banana/file"
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "banana" / "file").str()));
}

TEST_CASE("Matching directories with double asterisk") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "**" / "file").str()).value();
  REQUIRE_INCLUDE(pattern.match((root / "one").str()));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str()));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana" / "inner").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "two").str()));
}

TEST_CASE("Matching directories with trailing double asterisk") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "**").str()).value();
  REQUIRE_INCLUDE(pattern.match((root / "one").str()));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str()));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana" / "inner").str()));
}

TEST_CASE("Matching directories with single asterisk") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "*e*" / "file").str()).value();
  REQUIRE_INCLUDE(pattern.match((root / "one").str()));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "then").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "then" / "inner").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "not").str()));
}

TEST_CASE("Matching files without globs") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "banana" / "file").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str(), "file"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str(), "file"));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "banana").str(), "not_file"));
}

TEST_CASE("Matching files with single asterisk") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "banana" / "*.txt").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str(), "file"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str(), "file.txt"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str(), ".txt"));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "banana").str(), "file.jpg"));
}

TEST_CASE("Matching files with double asterisk in directory") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "**" / "banana" / "*.txt").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str(), "file.txt"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str(), "file.txt"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "inter" / "banana").str(), "other.txt"));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "inter" / "banana" / "not-good").str(), "other.txt"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "inter" / "inter2" / "banana").str(), "other.txt"));
}

TEST_CASE("Matching files with trailing double asterisk") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern((root / "one" / "**").str()).value();
  REQUIRE_INCLUDE(pattern.match((root / "one").str(), "file.txt"));
  REQUIRE_INCLUDE(pattern.match((root / "one" / "banana").str(), "file.txt"));
}

TEST_CASE("Matching directory with exclusion") {
  TestController controller;
  utils::Path root{controller.createTempDirectory()};
  // root
  //   |- file1.txt
  //   |- one
  //   |   |- file2.txt
  //   |   |- apple
  //   |   |   |- file3.jpg
  //   |   |   |- file4.txt
  //   |   |
  //   |   |- banana
  //   |        |- file5.txt
  //   |- two
  //       |- file6.txt
  fileutils::create_dir((root / "one" / "apple").str(), true);
  fileutils::create_dir((root / "one" / "banana").str(), true);
  fileutils::create_dir((root / "two").str(), true);

  auto file1 = (root / "file1.txt").str();
  auto file2 = (root / "one" / "file2.txt").str();
  auto file3 = (root / "one" / "apple" / "file3.jpg").str();
  auto file4 = (root / "one" / "apple" / "file4.txt").str();
  auto file5 = (root / "one" / "banana" / "file5.txt").str();
  auto file6 = (root / "two" / "file6.txt").str();

  std::set<std::string> files{file1, file2, file3, file4, file5, file6};

  for (const auto& file : files) {std::ofstream{file};}

  // match all
  auto matched_files = FileMatcher((root / "**").str()).listFiles();
  REQUIRE(matched_files == files);

  // match txt files
  matched_files = FileMatcher((root / "**" / "*.txt").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file1, file2, file4, file5, file6}));

  // match everything in /one, but not in /one/apple
  matched_files = FileMatcher((root / "one" / "**").str() + ",!" + (root / "one" / "apple" / "**").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file2, file5}));

  // match everything in /one/banana, and in /two
  matched_files = FileMatcher((root / "one" / "banana" / "**").str() + "," + (root / "two" / "**").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file5, file6}));

  // match everything in / not in /one, /two but in /one/apple
  matched_files = FileMatcher(
      root.str() +
      ",!" + (root / "one" / "**").str() +
      "," + (root / "one" / "apple" / "**").str() +
      ",!" + (root / "two" / "**").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file3, file4}));

  // exclude a single file
  matched_files = FileMatcher((root / "one" / "apple" / "**").str() + ",!" + (root / "one" / "apple" / "file3.jpg").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file4}));

  // exclude by file extension
  matched_files = FileMatcher((root / "**").str() + ",!" + (root / "**" / "*.txt").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file3}));

  // exclude files with name "*tx*" (everything except the jpg)
  matched_files = FileMatcher((root / "**").str() + ",!" + (root / "**" / "*tx*").str()).listFiles();
  REQUIRE((matched_files == std::set<std::string>{file3}));
}

TEST_CASE("Excluding directories with directory-tree exclusion") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern("!" + (root / "one" / "**").str()).value();
  REQUIRE_EXCLUDE(pattern.match((root / "one").str()));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "banana").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "two").str()));
}

TEST_CASE("Excluding files with directory-tree exclusion") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern("!" + (root / "one" / "**").str()).value();
  REQUIRE_EXCLUDE(pattern.match((root / "one").str(), "file.txt"));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "banana").str(), "other.txt"));
  REQUIRE_DONT_CARE(pattern.match((root / "two").str(), "no-excluded.txt"));
}

TEST_CASE("Excluding with specific file exclusion") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern("!" + (root / "one" / "banana" / "file.txt").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "one").str(), "file.txt"));
  REQUIRE_DONT_CARE(pattern.match((root / "two").str(), "no-excluded.txt"));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "banana").str(), "other.txt"));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "banana").str(), "file.txt"));
}

TEST_CASE("Excluding with file wildcards") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern("!" + (root / "one" / "banana" / "*.txt").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str()));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "banana").str(), "other.txt"));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "banana").str(), "file.txt"));
}

TEST_CASE("Excluding with directory-tree file specific exclusion") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern("!" + (root / "one" / "**" / "file.txt").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str()));
  REQUIRE_EXCLUDE(pattern.match((root / "one").str(), "file.txt"));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "banana").str(), "file.txt"));
}

TEST_CASE("Excluding with directory wildcard exclusion") {
#ifdef WIN32
  utils::Path root{"C:\\"};
#else
  utils::Path root{"/"};
#endif
  auto pattern = FilePattern::fromPattern("!" + (root / "one" / "*e*" / "*").str()).value();
  REQUIRE_DONT_CARE(pattern.match((root / "one").str()));
  // even though it seems to match, it would exclude the likes of "/one/ten/banana/file.txt"
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "ten").str()));
  REQUIRE_DONT_CARE(pattern.match((root / "one" / "six").str()));
  REQUIRE_EXCLUDE(pattern.match((root / "one" / "ten").str(), "file.txt"));
}
