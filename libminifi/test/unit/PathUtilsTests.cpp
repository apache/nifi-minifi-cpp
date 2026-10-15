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

#include <filesystem>
#include <string>
#include <string_view>
#include "unit/Catch.h"
#include "utils/file/PathUtils.h"

namespace fs = org::apache::nifi::minifi::utils::file;

namespace {

// Both path grammars are exercised on every host: a std::filesystem::path only carries the grammar
// of the platform it was compiled for, so without the flavor parameter the Windows rules below
// would only ever run on Windows CI -- which is exactly where they were broken.
std::string posixUri(const std::string_view generic_path) {
  const auto uri = fs::genericPathToUri(generic_path, fs::PathFlavor::Posix);
  REQUIRE(uri.has_value());
  return *uri;
}

std::string windowsUri(const std::string_view generic_path) {
  const auto uri = fs::genericPathToUri(generic_path, fs::PathFlavor::Windows);
  REQUIRE(uri.has_value());
  return *uri;
}

}  // namespace

TEST_CASE("file::globToRegex works", "[globToRegex]") {
  REQUIRE(fs::globToRegex("").empty());
  REQUIRE(fs::globToRegex("NoSpecialChars") == "NoSpecialChars");
  REQUIRE(fs::globToRegex("ReplaceDot.txt") == "ReplaceDot\\.txt");
  REQUIRE(fs::globToRegex("Replace.Multiple.Dots...txt") == "Replace\\.Multiple\\.Dots\\.\\.\\.txt");
  REQUIRE(fs::globToRegex("ReplaceAsterisk.*") == "ReplaceAsterisk\\..*");
  REQUIRE(fs::globToRegex("Replace*Multiple*Asterisks") == "Replace.*Multiple.*Asterisks");
  REQUIRE(fs::globToRegex("ReplaceQuestionMark?.txt") == "ReplaceQuestionMark.\\.txt");
}

TEST_CASE("file::genericPathToUri builds POSIX file URIs", "[pathToUri]") {
  SECTION("an absolute path gets the empty authority, so three slashes") {
    CHECK(posixUri("/home/user/file.txt") == "file:///home/user/file.txt");
    CHECK(posixUri("/") == "file:///");
  }

  SECTION("characters that would otherwise change the URI's meaning are escaped") {
    CHECK(posixUri("/tmp/a b#c%d?e") == "file:///tmp/a%20b%23c%25d%3Fe");
  }

  SECTION("non-ASCII bytes are percent-encoded one byte at a time") {
    CHECK(posixUri("/tmp/caf\xc3\xa9.txt") == "file:///tmp/caf%C3%A9.txt");
    // A POSIX file name is a byte string and need not be valid UTF-8. These bytes must survive
    // verbatim rather than being transcoded or replaced, or the URI names a different file.
    CHECK(posixUri("/tmp/\xffraw") == "file:///tmp/%FFraw");
  }

  SECTION("a backslash is an ordinary file name character on POSIX") {
    CHECK(posixUri("/tmp/back\\slash.txt") == "file:///tmp/back%5Cslash.txt");
  }

  SECTION("the unreserved set is kept literal and sub-delimiters are escaped") {
    CHECK(posixUri("/tmp/a-b_c.d~e") == "file:///tmp/a-b_c.d~e");
    CHECK(posixUri("/tmp/a+b&c(d)") == "file:///tmp/a%2Bb%26c%28d%29");
  }

  SECTION("a colon is a valid pchar and stays literal") {
    CHECK(posixUri("/tmp/a:b.txt") == "file:///tmp/a:b.txt");
  }

  SECTION("a colon in the first segment is not a Windows drive letter") {
    CHECK(posixUri("/:colonroot/f") == "file:///:colonroot/f");
  }

  SECTION("a leading double slash is part of the path, not an authority") {
    CHECK(posixUri("//share/dir/f") == "file:////share/dir/f");
  }

  SECTION("the path is passed through as given") {
    CHECK(posixUri("/tmp/../x/f") == "file:///tmp/../x/f");
    CHECK(posixUri("/tmp/dir/") == "file:///tmp/dir/");
  }

  SECTION("paths that are not absolute are rejected") {
    CHECK(!fs::genericPathToUri("rel/f", fs::PathFlavor::Posix).has_value());
    CHECK(!fs::genericPathToUri("", fs::PathFlavor::Posix).has_value());
  }
}

TEST_CASE("file::genericPathToUri builds Windows file URIs", "[pathToUri]") {
  SECTION("a drive-letter path keeps its colon and gains the empty authority") {
    CHECK(windowsUri("C:/Users/user/file.txt") == "file:///C:/Users/user/file.txt");
    CHECK(windowsUri("C:/") == "file:///C:/");
  }

  SECTION("the drive letter is upper-cased so one file yields one URI") {
    CHECK(windowsUri("c:/Temp/x") == "file:///C:/Temp/x");
  }

  SECTION("path characters are escaped the same way as on POSIX") {
    CHECK(windowsUri("C:/dir/a b.txt") == "file:///C:/dir/a%20b.txt");
    CHECK(windowsUri("C:/dir/\xe6\x97\xa5\xe6\x9c\xac.txt") == "file:///C:/dir/%E6%97%A5%E6%9C%AC.txt");
  }

  SECTION("a UNC path puts the host in the authority") {
    CHECK(windowsUri("//server/share/file.txt") == "file://server/share/file.txt");
    CHECK(windowsUri("//server/share") == "file://server/share");
  }

  SECTION("the host is lower-cased because host names are case-insensitive") {
    CHECK(windowsUri("//SERVER/share/f") == "file://server/share/f");
  }

  SECTION("an extended-length prefix is stripped rather than read as a host") {
    CHECK(windowsUri("//?/C:/long/p") == "file:///C:/long/p");
    CHECK(windowsUri("//?/UNC/server/share/f") == "file://server/share/f");
    CHECK(windowsUri("//./C:/x") == "file:///C:/x");
  }

  SECTION("paths that do not name a file are rejected") {
    // Device namespace: there is no file for a file URI to point at.
    CHECK(!fs::genericPathToUri("//./PhysicalDrive0", fs::PathFlavor::Windows).has_value());
    // Relative to the current directory *of drive C*, so not expressible without that state.
    CHECK(!fs::genericPathToUri("C:foo", fs::PathFlavor::Windows).has_value());
    // Rooted on the current drive, which is likewise unknown here.
    CHECK(!fs::genericPathToUri("/foo/bar", fs::PathFlavor::Windows).has_value());
    CHECK(!fs::genericPathToUri("rel/f", fs::PathFlavor::Windows).has_value());
    CHECK(!fs::genericPathToUri("//", fs::PathFlavor::Windows).has_value());
    CHECK(!fs::genericPathToUri("", fs::PathFlavor::Windows).has_value());
  }
}

TEST_CASE("file::pathToUri converts native paths", "[pathToUri]") {
  SECTION("a relative path is resolved against the current directory") {
    // Compared against the explicitly-resolved path rather than a literal, so the test does not
    // depend on where it is run from.
    CHECK(fs::pathToUri("rel/f.txt") == fs::pathToUri(std::filesystem::current_path() / "rel/f.txt"));
  }

  SECTION("an empty path has no URI") {
    CHECK(!fs::pathToUri("").has_value());
  }

  SECTION("a native absolute path round-trips through the native flavor") {
#ifdef WIN32
    CHECK(fs::pathToUri(R"(C:\dir\a b.txt)") == "file:///C:/dir/a%20b.txt");
    CHECK(fs::pathToUri(R"(\\server\share\f.txt)") == "file://server/share/f.txt");
#else
    CHECK(fs::pathToUri("/tmp/dir/a b.txt") == "file:///tmp/dir/a%20b.txt");
#endif
  }

#ifdef WIN32
  SECTION("characters outside the ANSI code page are encoded as UTF-8") {
    // path::generic_string() would convert UTF-16 through the ANSI code page and replace these
    // characters with '?', so the URI has to be built from the wide string instead. This cannot be
    // expressed through genericPathToUri: the loss happens inside path's own conversion.
    CHECK(fs::pathToUri(std::filesystem::path{L"C:\\dir\\日本.txt"}) == "file:///C:/dir/%E6%97%A5%E6%9C%AC.txt");
  }
#endif
}
