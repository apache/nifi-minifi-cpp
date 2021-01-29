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
#include <thread>
#include "../TestBase.h"
#include "core/Core.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/gsl.h"
#include "utils/Environment.h"
#include "utils/TimeUtil.h"

namespace FileUtils = org::apache::nifi::minifi::utils::file;

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

TEST_CASE("TestFileUtils::get_child_path", "[TestGetChildPath]") {
#ifdef WIN32
  REQUIRE("bar" == FileUtils::get_child_path("foo\\bar"));
  REQUIRE("bar\\" == FileUtils::get_child_path("foo\\bar\\"));
  REQUIRE("bar" == FileUtils::get_child_path("C:\\foo\\bar"));
  REQUIRE("bar\\" == FileUtils::get_child_path("C:\\foo\\bar\\"));
  REQUIRE("foo" == FileUtils::get_child_path("C:\\foo"));
  REQUIRE("foo\\" == FileUtils::get_child_path("C:\\foo\\"));
  REQUIRE("" == FileUtils::get_child_path("C:\\"));
  REQUIRE("" == FileUtils::get_child_path("C:\\\\"));
#else
  REQUIRE("bar" == FileUtils::get_child_path("foo/bar"));
  REQUIRE("bar/" == FileUtils::get_child_path("foo/bar/"));
  REQUIRE("bar" == FileUtils::get_child_path("/foo/bar"));
  REQUIRE("bar/" == FileUtils::get_child_path("/foo/bar/"));
  REQUIRE("foo" == FileUtils::get_child_path("/foo"));
  REQUIRE("foo/" == FileUtils::get_child_path("/foo/"));
  REQUIRE("" == FileUtils::get_child_path("/"));
  REQUIRE("" == FileUtils::get_child_path("//"));
#endif
}

TEST_CASE("TestFilePath", "[TestGetFileNameAndPath]") {
  SECTION("VALID FILE AND PATH") {
  std::stringstream path;
  path << "a" << FileUtils::get_separator() << "b" << FileUtils::get_separator() << "c";
  std::stringstream file;
  file << path.str() << FileUtils::get_separator() << "file";
  std::string filename, filepath;
  REQUIRE(true == utils::file::getFileNameAndPath(file.str(), filepath, filename) );
  REQUIRE(path.str() == filepath);
  REQUIRE("file" == filename);
}
SECTION("NO FILE VALID PATH") {
  std::stringstream path;
  path << "a" << FileUtils::get_separator() << "b" << FileUtils::get_separator() << "c" << FileUtils::get_separator();
  std::string filename, filepath;
  REQUIRE(false == utils::file::getFileNameAndPath(path.str(), filepath, filename) );
  REQUIRE(filepath.empty());
  REQUIRE(filename.empty());
}
SECTION("FILE NO PATH") {
  std::stringstream path;
  path << FileUtils::get_separator() << "file";
  std::string filename, filepath;
  std::string expectedPath;
  expectedPath += FileUtils::get_separator();
  REQUIRE(true == utils::file::getFileNameAndPath(path.str(), filepath, filename) );
  REQUIRE(expectedPath == filepath);
  REQUIRE("file" == filename);
}
SECTION("NO FILE NO PATH") {
  std::string path = "file";
  std::string filename, filepath;
  REQUIRE(false == utils::file::getFileNameAndPath(path, filepath, filename) );
  REQUIRE(filepath.empty());
  REQUIRE(filename.empty());
}
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

TEST_CASE("TestFileUtils::create_dir", "[TestCreateDir]") {
  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::string test_dir_path = std::string(dir) + FileUtils::get_separator() + "random_dir";

  REQUIRE(FileUtils::create_dir(test_dir_path, false) == 0);  // Dir has to be created successfully
  struct stat buffer;
  REQUIRE(stat(test_dir_path.c_str(), &buffer) == 0);  // Check if directory exists
  REQUIRE(FileUtils::create_dir(test_dir_path, false) == 0);  // Dir already exists, success should be returned
  REQUIRE(FileUtils::delete_dir(test_dir_path, false) == 0);  // Delete should be successful as well
  test_dir_path += "/random_dir2";
  REQUIRE(FileUtils::create_dir(test_dir_path, false) != 0);  // Create dir should fail for multiple directories if recursive option is not set
}

TEST_CASE("TestFileUtils::create_dir recursively", "[TestCreateDir]") {
  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::string test_dir_path = std::string(dir) + FileUtils::get_separator() + "random_dir" + FileUtils::get_separator() +
    "random_dir2" + FileUtils::get_separator() + "random_dir3";

  REQUIRE(FileUtils::create_dir(test_dir_path) == 0);  // Dir has to be created successfully
  struct stat buffer;
  REQUIRE(stat(test_dir_path.c_str(), &buffer) == 0);  // Check if directory exists
  REQUIRE(FileUtils::create_dir(test_dir_path) == 0);  // Dir already exists, success should be returned
  REQUIRE(FileUtils::delete_dir(test_dir_path) == 0);  // Delete should be successful as well
}

TEST_CASE("TestFileUtils::getFullPath", "[TestGetFullPath]") {
  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  const std::string tempDir = utils::file::getFullPath(testController.createTempDirectory(format));

  const std::string cwd = utils::Environment::getCurrentWorkingDirectory();

  REQUIRE(utils::Environment::setCurrentWorkingDirectory(tempDir.c_str()));
  const auto cwdGuard = gsl::finally([&cwd]() {
    utils::Environment::setCurrentWorkingDirectory(cwd.c_str());
  });

  const std::string tempDir1 = utils::file::FileUtils::concat_path(tempDir, "test1");
  const std::string tempDir2 = utils::file::FileUtils::concat_path(tempDir, "test2");
  REQUIRE(0 == utils::file::FileUtils::create_dir(tempDir1));
  REQUIRE(0 == utils::file::FileUtils::create_dir(tempDir2));

  REQUIRE(tempDir1 == utils::file::getFullPath(tempDir1));
  REQUIRE(tempDir1 == utils::file::getFullPath("test1"));
  REQUIRE(tempDir1 == utils::file::getFullPath("./test1"));
  REQUIRE(tempDir1 == utils::file::getFullPath("././test1"));
  REQUIRE(tempDir1 == utils::file::getFullPath("./test2/../test1"));
#ifdef WIN32
  REQUIRE(tempDir1 == utils::file::getFullPath(".\\test1"));
  REQUIRE(tempDir1 == utils::file::getFullPath(".\\.\\test1"));
  REQUIRE(tempDir1 == utils::file::getFullPath(".\\test2\\..\\test1"));
#endif
}

TEST_CASE("FileUtils::last_write_time and last_write_time_point work", "[last_write_time][last_write_time_point]") {
  using namespace std::chrono;

  uint64_t time_before_write = utils::timeutils::getTimeMillis() / 1000;
  time_point<system_clock, seconds> time_point_before_write = time_point_cast<seconds>(system_clock::now());

  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  std::string dir = testController.createTempDirectory(format);

  std::string test_file = dir + FileUtils::get_separator() + "test.txt";
  REQUIRE(FileUtils::last_write_time(test_file) == 0);
  REQUIRE(FileUtils::last_write_time_point(test_file) == (time_point<system_clock, seconds>{}));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::ofstream test_file_stream(test_file);
  test_file_stream << "foo\n";
  test_file_stream.flush();

  uint64_t time_after_first_write = utils::timeutils::getTimeMillis() / 1000;
  time_point<system_clock, seconds> time_point_after_first_write = time_point_cast<seconds>(system_clock::now());

  uint64_t first_mtime = FileUtils::last_write_time(test_file);
  REQUIRE(first_mtime >= time_before_write);
  REQUIRE(first_mtime <= time_after_first_write);

  time_point<system_clock, seconds> first_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(first_mtime_time_point >= time_point_before_write);
  REQUIRE(first_mtime_time_point <= time_point_after_first_write);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  test_file_stream << "bar\n";
  test_file_stream.flush();

  uint64_t time_after_second_write = utils::timeutils::getTimeMillis() / 1000;
  time_point<system_clock, seconds> time_point_after_second_write = time_point_cast<seconds>(system_clock::now());

  uint64_t second_mtime = FileUtils::last_write_time(test_file);
  REQUIRE(second_mtime >= first_mtime);
  REQUIRE(second_mtime >= time_after_first_write);
  REQUIRE(second_mtime <= time_after_second_write);

  time_point<system_clock, seconds> second_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(second_mtime_time_point >= first_mtime_time_point);
  REQUIRE(second_mtime_time_point >= time_point_after_first_write);
  REQUIRE(second_mtime_time_point <= time_point_after_second_write);

  test_file_stream.close();

  // On Windows it would rarely occur that the last_write_time is off by 1 from the previous check
#ifndef WIN32
  uint64_t third_mtime = FileUtils::last_write_time(test_file);
  REQUIRE(third_mtime == second_mtime);

  time_point<system_clock, seconds> third_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(third_mtime_time_point == second_mtime_time_point);
#endif
}

TEST_CASE("FileUtils::file_size works", "[file_size]") {
  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  std::string dir = testController.createTempDirectory(format);

  std::string test_file = dir + FileUtils::get_separator() + "test.txt";
  REQUIRE(FileUtils::file_size(test_file) == 0);

  std::ofstream test_file_stream(test_file, std::ios::out | std::ios::binary);
  test_file_stream << "foo\n";
  test_file_stream.flush();

  REQUIRE(FileUtils::file_size(test_file) == 4);

  test_file_stream << "foobar\n";
  test_file_stream.flush();

  REQUIRE(FileUtils::file_size(test_file) == 11);

  test_file_stream.close();

  REQUIRE(FileUtils::file_size(test_file) == 11);
}

TEST_CASE("FileUtils::computeChecksum works", "[computeChecksum]") {
  constexpr uint64_t CHECKSUM_OF_0_BYTES = 0u;
  constexpr uint64_t CHECKSUM_OF_4_BYTES = 2117232040u;
  constexpr uint64_t CHECKSUM_OF_11_BYTES = 3461392622u;

  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  std::string dir = testController.createTempDirectory(format);

  std::string test_file = dir + FileUtils::get_separator() + "test.txt";
  REQUIRE(FileUtils::computeChecksum(test_file, 0) == CHECKSUM_OF_0_BYTES);

  std::ofstream test_file_stream{test_file, std::ios::out | std::ios::binary};
  test_file_stream << "foo\n";
  test_file_stream.flush();

  REQUIRE(FileUtils::computeChecksum(test_file, 4) == CHECKSUM_OF_4_BYTES);

  test_file_stream << "foobar\n";
  test_file_stream.flush();

  REQUIRE(FileUtils::computeChecksum(test_file, 11) == CHECKSUM_OF_11_BYTES);

  test_file_stream.close();

  REQUIRE(FileUtils::computeChecksum(test_file, 0) == CHECKSUM_OF_0_BYTES);
  REQUIRE(FileUtils::computeChecksum(test_file, 4) == CHECKSUM_OF_4_BYTES);
  REQUIRE(FileUtils::computeChecksum(test_file, 11) == CHECKSUM_OF_11_BYTES);


  std::string another_file = dir + FileUtils::get_separator() + "another_test.txt";
  REQUIRE(FileUtils::computeChecksum(test_file, 0) == CHECKSUM_OF_0_BYTES);

  std::ofstream another_file_stream{another_file, std::ios::out | std::ios::binary};
  another_file_stream << "foo\nfoobar\nbaz\n";   // starts with the same bytes as test_file
  another_file_stream.close();

  REQUIRE(FileUtils::computeChecksum(another_file, 0) == CHECKSUM_OF_0_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 4) == CHECKSUM_OF_4_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 11) == CHECKSUM_OF_11_BYTES);
}

TEST_CASE("FileUtils::computeChecksum with large files", "[computeChecksum]") {
  constexpr uint64_t CHECKSUM_OF_0_BYTES = 0u;
  constexpr uint64_t CHECKSUM_OF_4095_BYTES = 1902799545u;
  constexpr uint64_t CHECKSUM_OF_4096_BYTES = 1041266625u;
  constexpr uint64_t CHECKSUM_OF_4097_BYTES = 1619129554u;
  constexpr uint64_t CHECKSUM_OF_8192_BYTES = 305726917u;

  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  std::string dir = testController.createTempDirectory(format);

  std::string test_file = dir + FileUtils::get_separator() + "test.txt";
  REQUIRE(FileUtils::computeChecksum(test_file, 0) == CHECKSUM_OF_0_BYTES);

  std::ofstream test_file_stream{test_file, std::ios::out | std::ios::binary};
  test_file_stream << std::string(4096, 'x');
  test_file_stream.flush();

  REQUIRE(FileUtils::computeChecksum(test_file, 4095) == CHECKSUM_OF_4095_BYTES);
  REQUIRE(FileUtils::computeChecksum(test_file, 4096) == CHECKSUM_OF_4096_BYTES);

  test_file_stream << 'x';
  test_file_stream.flush();

  REQUIRE(FileUtils::computeChecksum(test_file, 4097) == CHECKSUM_OF_4097_BYTES);

  test_file_stream.close();

  REQUIRE(FileUtils::computeChecksum(test_file, 0) == CHECKSUM_OF_0_BYTES);
  REQUIRE(FileUtils::computeChecksum(test_file, 4095) == CHECKSUM_OF_4095_BYTES);
  REQUIRE(FileUtils::computeChecksum(test_file, 4096) == CHECKSUM_OF_4096_BYTES);
  REQUIRE(FileUtils::computeChecksum(test_file, 4097) == CHECKSUM_OF_4097_BYTES);


  std::string another_file = dir + FileUtils::get_separator() + "another_test.txt";
  REQUIRE(FileUtils::computeChecksum(test_file, 0) == CHECKSUM_OF_0_BYTES);

  std::ofstream another_file_stream{another_file, std::ios::out | std::ios::binary};
  another_file_stream << std::string(8192, 'x');   // starts with the same bytes as test_file
  another_file_stream.close();

  REQUIRE(FileUtils::computeChecksum(another_file, 0) == CHECKSUM_OF_0_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 4095) == CHECKSUM_OF_4095_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 4096) == CHECKSUM_OF_4096_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 4097) == CHECKSUM_OF_4097_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 8192) == CHECKSUM_OF_8192_BYTES);
  REQUIRE(FileUtils::computeChecksum(another_file, 9000) == CHECKSUM_OF_8192_BYTES);
}

#ifndef WIN32
TEST_CASE("FileUtils::set_permissions", "[TestSetPermissions]") {
  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto path = dir + FileUtils::get_separator() + "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);

  REQUIRE(FileUtils::set_permissions(path, 0644) == 0);
  uint32_t perms;
  REQUIRE(FileUtils::get_permissions(path, perms));
  REQUIRE(perms == 0644);
}
#endif

TEST_CASE("FileUtils::exists", "[TestExists]") {
  TestController testController;

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  auto path = dir + FileUtils::get_separator() + "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);
  auto invalid_path = dir + FileUtils::get_separator() + "test_file2.txt";

  REQUIRE(FileUtils::exists(path));
  REQUIRE(!FileUtils::exists(invalid_path));
}

TEST_CASE("TestFileUtils::delete_dir should fail with empty path", "[TestEmptyDeleteDir]") {
  TestController testController;
  REQUIRE(FileUtils::delete_dir("") != 0);
  REQUIRE(FileUtils::delete_dir("", false) != 0);
}
