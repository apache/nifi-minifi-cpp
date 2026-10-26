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

TEST_CASE("file::genericPathToUri gives a POSIX path the empty authority", "[pathToUri]") {
  CHECK(posixUri("/home/user/file.txt") == "file:///home/user/file.txt");
  CHECK(posixUri("/") == "file:///");
}

TEST_CASE("file::genericPathToUri escapes POSIX characters that would change the URI's meaning", "[pathToUri]") {
  CHECK(posixUri("/tmp/a b#c%d?e") == "file:///tmp/a%20b%23c%25d%3Fe");
}

TEST_CASE("file::genericPathToUri percent-encodes POSIX non-ASCII one byte at a time", "[pathToUri]") {
  CHECK(posixUri("/tmp/caf\xc3\xa9.txt") == "file:///tmp/caf%C3%A9.txt");
  // A POSIX file name is a byte string and need not be valid UTF-8. These bytes must survive
  // verbatim rather than being transcoded or replaced, or the URI names a different file.
  CHECK(posixUri("/tmp/\xffraw") == "file:///tmp/%FFraw");
}

TEST_CASE("file::genericPathToUri treats a backslash as an ordinary POSIX file name character", "[pathToUri]") {
  CHECK(posixUri("/tmp/back\\slash.txt") == "file:///tmp/back%5Cslash.txt");
}

TEST_CASE("file::genericPathToUri keeps the unreserved set literal and escapes sub-delimiters", "[pathToUri]") {
  CHECK(posixUri("/tmp/a-b_c.d~e") == "file:///tmp/a-b_c.d~e");
  CHECK(posixUri("/tmp/a+b&c(d)") == "file:///tmp/a%2Bb%26c%28d%29");
}

TEST_CASE("file::genericPathToUri keeps a colon literal, since it is a valid pchar", "[pathToUri]") {
  CHECK(posixUri("/tmp/a:b.txt") == "file:///tmp/a:b.txt");
}

TEST_CASE("file::genericPathToUri does not read a leading POSIX colon as a drive letter", "[pathToUri]") {
  CHECK(posixUri("/:colonroot/f") == "file:///:colonroot/f");
}

TEST_CASE("file::genericPathToUri keeps a leading POSIX double slash in the path", "[pathToUri]") {
  CHECK(posixUri("//share/dir/f") == "file:////share/dir/f");
}

TEST_CASE("file::genericPathToUri passes a POSIX path through as given", "[pathToUri]") {
  CHECK(posixUri("/tmp/../x/f") == "file:///tmp/../x/f");
  CHECK(posixUri("/tmp/dir/") == "file:///tmp/dir/");
}

TEST_CASE("file::genericPathToUri rejects a POSIX path that is not absolute", "[pathToUri]") {
  CHECK(!fs::genericPathToUri("rel/f", fs::PathFlavor::Posix).has_value());
  CHECK(!fs::genericPathToUri("", fs::PathFlavor::Posix).has_value());
}

TEST_CASE("file::genericPathToUri keeps the colon in a Windows drive-letter path", "[pathToUri]") {
  CHECK(windowsUri("C:/Users/user/file.txt") == "file:///C:/Users/user/file.txt");
  CHECK(windowsUri("C:/") == "file:///C:/");
}

TEST_CASE("file::genericPathToUri upper-cases the Windows drive letter, so one file yields one URI", "[pathToUri]") {
  CHECK(windowsUri("c:/Temp/x") == "file:///C:/Temp/x");
}

TEST_CASE("file::genericPathToUri escapes Windows path characters the same way as POSIX ones", "[pathToUri]") {
  CHECK(windowsUri("C:/dir/a b.txt") == "file:///C:/dir/a%20b.txt");
  CHECK(windowsUri("C:/dir/\xe6\x97\xa5\xe6\x9c\xac.txt") == "file:///C:/dir/%E6%97%A5%E6%9C%AC.txt");
}

TEST_CASE("file::genericPathToUri puts a UNC host in the authority", "[pathToUri]") {
  CHECK(windowsUri("//server/share/file.txt") == "file://server/share/file.txt");
  CHECK(windowsUri("//server/share") == "file://server/share");
}

TEST_CASE("file::genericPathToUri lower-cases the UNC host, which is case-insensitive", "[pathToUri]") {
  CHECK(windowsUri("//SERVER/share/f") == "file://server/share/f");
}

TEST_CASE("file::genericPathToUri strips a Windows extended-length prefix rather than reading it as a host", "[pathToUri]") {
  CHECK(windowsUri("//?/C:/long/p") == "file:///C:/long/p");
  CHECK(windowsUri("//?/UNC/server/share/f") == "file://server/share/f");
  CHECK(windowsUri("//./C:/x") == "file:///C:/x");
}

TEST_CASE("file::genericPathToUri rejects Windows paths that do not name a file", "[pathToUri]") {
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

TEST_CASE("file::pathToUri resolves a relative path against the current directory", "[pathToUri]") {
  // Compared against the explicitly-resolved path rather than a literal, so the test does not
  // depend on where it is run from.
  CHECK(fs::pathToUri("rel/f.txt") == fs::pathToUri(std::filesystem::current_path() / "rel/f.txt"));
}

TEST_CASE("file::pathToUri rejects an empty path", "[pathToUri]") {
  CHECK(!fs::pathToUri("").has_value());
}

TEST_CASE("file::pathToUri converts a native absolute path", "[pathToUri]") {
#ifdef WIN32
  CHECK(fs::pathToUri(R"(C:\dir\a b.txt)") == "file:///C:/dir/a%20b.txt");
  CHECK(fs::pathToUri(R"(\\server\share\f.txt)") == "file://server/share/f.txt");
#else
  CHECK(fs::pathToUri("/tmp/dir/a b.txt") == "file:///tmp/dir/a%20b.txt");
#endif
}

#ifdef WIN32
TEST_CASE("file::pathToUri encodes characters outside the ANSI code page as UTF-8", "[pathToUri]") {
  // path::generic_string() would convert UTF-16 through the ANSI code page and replace these
  // characters with '?', so the URI has to be built from the wide string instead. This cannot be
  // expressed through genericPathToUri: the loss happens inside path's own conversion.
  CHECK(fs::pathToUri(std::filesystem::path{L"C:\\dir\\日本.txt"}) == "file:///C:/dir/%E6%97%A5%E6%9C%AC.txt");
}
#endif
