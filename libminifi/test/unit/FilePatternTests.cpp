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

#include <filesystem>

#include "../TestBase.h"
#include "utils/file/FilePattern.h"
#include "range/v3/view/transform.hpp"
#include "range/v3/view/map.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/view/cache1.hpp"
#include "range/v3/view/c_str.hpp"
#include "range/v3/range/conversion.hpp"

struct FilePatternTestAccessor {
  using FilePatternSegment = fileutils::FilePattern::FilePatternSegment;
};

using FilePatternSegment = FilePatternTestAccessor::FilePatternSegment;
using FilePattern = fileutils::FilePattern;

#define REQUIRE_INCLUDE(val) REQUIRE((val == FilePatternSegment::MatchResult::INCLUDE))
#define REQUIRE_EXCLUDE(val) REQUIRE((val == FilePatternSegment::MatchResult::EXCLUDE))
#define REQUIRE_NOT_MATCHING(val) REQUIRE((val == FilePatternSegment::MatchResult::NOT_MATCHING))

#ifdef WIN32
static std::filesystem::path root{"C:\\"};
#else
static std::filesystem::path root{"/"};
#endif

TEST_CASE("Invalid paths") {
  REQUIRE_THROWS(FilePatternSegment(""));
  REQUIRE_THROWS(FilePatternSegment("."));
  REQUIRE_THROWS(FilePatternSegment(".."));
  REQUIRE_THROWS(FilePatternSegment("!"));
  REQUIRE_THROWS(FilePatternSegment("!."));
  REQUIRE_THROWS(FilePatternSegment("!.."));
  // parent accessor after wildcard
  FilePatternSegment("./../file.txt");  // sanity check
  REQUIRE_THROWS(FilePatternSegment("./*/../file.txt"));
}

TEST_CASE("FilePattern reports error in correct subpattern") {
  std::vector<std::pair<std::string, std::string>> invalid_subpattern{
      {"", "Empty pattern"},
      {".", "Invalid file pattern '.'"},
      {"..", "Invalid file pattern '..'"},
      {"!./*/../file.txt", "Parent accessor is not supported after wildcards"},
      {"./", "Empty file pattern"}
  };
  auto pattern = invalid_subpattern | ranges::views::keys | ranges::views::join(',') | ranges::to<std::string>;
  size_t idx = 0;
  FilePattern(pattern, [&] (std::string_view subpattern, std::string_view error) {
    REQUIRE(invalid_subpattern[idx].first == subpattern);
    REQUIRE(invalid_subpattern[idx++].second == error);
  });
  REQUIRE(idx == invalid_subpattern.size());
}

TEST_CASE("Matching directories without globs") {
  FilePatternSegment pattern((root / "one" / "banana" / "file").string());
  REQUIRE_INCLUDE(pattern.match(root / "one/"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "two/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "apple/"));
  // this looks at the DIRECTORY "/one/banana/file", while our pattern
  // looks for the FILE "/one/banana/file"
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "banana" / "file/"));
}

TEST_CASE("Matching directories with double asterisk") {
  FilePatternSegment pattern((root / "one" / "**" / "file").string());
  REQUIRE_INCLUDE(pattern.match(root / "one/"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana/"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / "inner/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "two/"));
}

TEST_CASE("Matching directories with trailing double asterisk") {
  FilePatternSegment pattern((root / "one" / "**").string());
  REQUIRE_INCLUDE(pattern.match(root / "one/"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana/"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / "inner/"));
}

TEST_CASE("Matching directories with single asterisk") {
  FilePatternSegment pattern((root / "one" / "*e*" / "file").string());
  REQUIRE_INCLUDE(pattern.match(root / "one/"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "then/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "then" / "inner/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "not/"));
}

TEST_CASE("Matching files without globs") {
  FilePatternSegment pattern((root / "one" / "banana" / "file").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "file"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / "file"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "banana" / "not_file"));
}

TEST_CASE("Matching files with single asterisk") {
  FilePatternSegment pattern((root / "one" / "banana" / "*.txt").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "file"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / "file.txt"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / ".txt"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "banana" / "file.jpg"));
}

TEST_CASE("Matching files with double asterisk in directory") {
  FilePatternSegment pattern((root / "one" / "**" / "banana" / "*.txt").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "file.txt"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / "file.txt"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "inter" / "banana" / "other.txt"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "inter" / "banana" / "not-good" / "other.txt"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "inter" / "inter2" / "banana" / "other.txt"));
}

TEST_CASE("Matching files with trailing double asterisk") {
  FilePatternSegment pattern((root / "one" / "**").string());
  REQUIRE_INCLUDE(pattern.match(root / "one" / "file.txt"));
  REQUIRE_INCLUDE(pattern.match(root / "one" / "banana" / "file.txt"));
}

TEST_CASE("Matching directory with exclusion") {
  TestController controller;
  std::filesystem::path root{controller.createTempDirectory()};
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
  fileutils::create_dir((root / "one" / "apple").string(), true);
  fileutils::create_dir((root / "one" / "banana").string(), true);
  fileutils::create_dir((root / "two").string(), true);

  auto file1 = root / "file1.txt";
  auto file2 = root / "one" / "file2.txt";
  auto file3 = root / "one" / "apple" / "file3.jpg";
  auto file4 = root / "one" / "apple" / "file4.txt";
  auto file5 = root / "one" / "banana" / "file5.txt";
  auto file6 = root / "two" / "file6.txt";

  std::set<std::filesystem::path> files{file1, file2, file3, file4, file5, file6};

  for (const auto& file : files) {std::ofstream{file};}

  // match all
  auto matched_files = fileutils::match(FilePattern((root / "**").string()));
  REQUIRE(matched_files == files);

  // match txt files
  matched_files = fileutils::match(FilePattern((root / "**" / "*.txt").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file1, file2, file4, file5, file6}));

  // match everything in /one, but not in /one/apple
  matched_files = fileutils::match(FilePattern((root / "one" / "**").string() + ",!" + (root / "one" / "apple" / "**").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file2, file5}));

  // match everything in /one/banana, and in /two
  matched_files = fileutils::match(FilePattern((root / "one" / "banana" / "**").string() + "," + (root / "two" / "**").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file5, file6}));

  // match everything in / not in /one, /two but in /one/apple
  matched_files = fileutils::match(FilePattern(
      root.string() +
      ",!" + (root / "one" / "**").string() +
      "," + (root / "one" / "apple" / "**").string() +
      ",!" + (root / "two" / "**").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file3, file4}));

  // exclude a single file
  matched_files = fileutils::match(FilePattern((root / "one" / "apple" / "**").string() + ",!" + (root / "one" / "apple" / "file3.jpg").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file4}));

  // exclude by file extension
  matched_files = fileutils::match(FilePattern((root / "**").string() + ",!" + (root / "**" / "*.txt").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file3}));

  // exclude files with name "*tx*" (everything except the jpg)
  matched_files = fileutils::match(FilePattern((root / "**").string() + ",!" + (root / "**" / "*tx*").string()));
  REQUIRE((matched_files == std::set<std::filesystem::path>{file3}));
}

TEST_CASE("Excluding directories with directory-tree exclusion") {
  FilePatternSegment pattern("!" + (root / "one" / "**").string());
  REQUIRE_EXCLUDE(pattern.match(root / "one/"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "banana/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "two/"));
}

TEST_CASE("Excluding files with directory-tree exclusion") {
  FilePatternSegment pattern("!" + (root / "one" / "**").string());
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "file.txt"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "banana" / "other.txt"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "two" / "no-excluded.txt"));
}

TEST_CASE("Excluding with specific file exclusion") {
  FilePatternSegment pattern("!" + (root / "one" / "banana" / "file.txt").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "file.txt"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "two" / "no-excluded.txt"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "banana" / "other.txt"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "banana" / "file.txt"));
}

TEST_CASE("Excluding with file wildcards") {
  FilePatternSegment pattern("!" + (root / "one" / "banana" / "*.txt").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one/"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "banana" / "other.txt"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "banana" / "file.txt"));
}

TEST_CASE("Excluding with directory-tree file specific exclusion") {
  FilePatternSegment pattern("!" + (root / "one" / "**" / "file.txt").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one/"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "file.txt"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "banana" / "file.txt"));
}

TEST_CASE("Excluding with directory wildcard exclusion") {
  FilePatternSegment pattern("!" + (root / "one" / "*e*" / "*").string());
  REQUIRE_NOT_MATCHING(pattern.match(root / "one/"));
  // even though it seems to match, it would exclude the likes of "/one/ten/banana/file.txt"
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "ten/"));
  REQUIRE_NOT_MATCHING(pattern.match(root / "one" / "six/"));
  REQUIRE_EXCLUDE(pattern.match(root / "one" / "ten" / "file.txt"));
}
