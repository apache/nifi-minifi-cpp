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

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include "../TestBase.h"
#include "../Catch.h"
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
  std::string base2 = "foo\\bar\\";
  REQUIRE("foo\\bar\\baz" == FileUtils::concat_path(base2, child));
#else
  std::string base = "foo/bar";
  REQUIRE("foo/bar/baz" == FileUtils::concat_path(base, child));
  std::string base2 = "foo/bar/";
  REQUIRE("foo/bar/baz" == FileUtils::concat_path(base2, child));
#endif
  REQUIRE(base + FileUtils::get_separator() + child == FileUtils::concat_path(base, child));
}

TEST_CASE("TestFileUtils::get_parent_path", "[TestGetParentPath]") {
#ifdef WIN32
  REQUIRE("foo\\" == FileUtils::get_parent_path("foo\\bar"));
  REQUIRE("foo\\" == FileUtils::get_parent_path("foo\\bar\\"));
  REQUIRE("C:\\foo\\" == FileUtils::get_parent_path("C:\\foo\\bar"));
  REQUIRE("C:\\foo\\" == FileUtils::get_parent_path("C:\\foo\\bar\\"));
  REQUIRE("C:\\" == FileUtils::get_parent_path("C:\\foo"));
  REQUIRE("C:\\" == FileUtils::get_parent_path("C:\\foo\\"));
  REQUIRE("" == FileUtils::get_parent_path("C:\\"));  // NOLINT(readability-container-size-empty)
  REQUIRE("" == FileUtils::get_parent_path("C:\\\\"));  // NOLINT(readability-container-size-empty)
#else
  REQUIRE("foo/" == FileUtils::get_parent_path("foo/bar"));
  REQUIRE("foo/" == FileUtils::get_parent_path("foo/bar/"));
  REQUIRE("/foo/" == FileUtils::get_parent_path("/foo/bar"));
  REQUIRE("/foo/" == FileUtils::get_parent_path("/foo/bar/"));
  REQUIRE("/" == FileUtils::get_parent_path("/foo"));
  REQUIRE("/" == FileUtils::get_parent_path("/foo/"));
  REQUIRE("" == FileUtils::get_parent_path("/"));  // NOLINT(readability-container-size-empty)
  REQUIRE("" == FileUtils::get_parent_path("//"));  // NOLINT(readability-container-size-empty)
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
  REQUIRE("" == FileUtils::get_child_path("C:\\"));  // NOLINT(readability-container-size-empty)
  REQUIRE("" == FileUtils::get_child_path("C:\\\\"));  // NOLINT(readability-container-size-empty)
#else
  REQUIRE("bar" == FileUtils::get_child_path("foo/bar"));
  REQUIRE("bar/" == FileUtils::get_child_path("foo/bar/"));
  REQUIRE("bar" == FileUtils::get_child_path("/foo/bar"));
  REQUIRE("bar/" == FileUtils::get_child_path("/foo/bar/"));
  REQUIRE("foo" == FileUtils::get_child_path("/foo"));
  REQUIRE("foo/" == FileUtils::get_child_path("/foo/"));
  REQUIRE("" == FileUtils::get_child_path("/"));  // NOLINT(readability-container-size-empty)
  REQUIRE("" == FileUtils::get_child_path("//"));  // NOLINT(readability-container-size-empty)
#endif
}

TEST_CASE("TestFilePath", "[TestGetFileNameAndPath]") {
  SECTION("VALID FILE AND PATH") {
  std::stringstream path;
  path << "a" << FileUtils::get_separator() << "b" << FileUtils::get_separator() << "c";
  std::stringstream file;
  file << path.str() << FileUtils::get_separator() << "file";
  std::string filename;
  std::string filepath;
  REQUIRE(true == utils::file::getFileNameAndPath(file.str(), filepath, filename) );
  REQUIRE(path.str() == filepath);
  REQUIRE("file" == filename);
}
SECTION("NO FILE VALID PATH") {
  std::stringstream path;
  path << "a" << FileUtils::get_separator() << "b" << FileUtils::get_separator() << "c" << FileUtils::get_separator();
  std::string filename;
  std::string filepath;
  REQUIRE(false == utils::file::getFileNameAndPath(path.str(), filepath, filename) );
  REQUIRE(filepath.empty());
  REQUIRE(filename.empty());
}
SECTION("FILE NO PATH") {
  std::stringstream path;
  path << FileUtils::get_separator() << "file";
  std::string filename;
  std::string filepath;
  std::string expectedPath;
  expectedPath += FileUtils::get_separator();
  REQUIRE(true == utils::file::getFileNameAndPath(path.str(), filepath, filename) );
  REQUIRE(expectedPath == filepath);
  REQUIRE("file" == filename);
}
SECTION("NO FILE NO PATH") {
  std::string path = "file";
  std::string filename;
  std::string filepath;
  REQUIRE(false == utils::file::getFileNameAndPath(path, filepath, filename) );
  REQUIRE(filepath.empty());
  REQUIRE(filename.empty());
}
}

TEST_CASE("TestFileUtils::get_executable_path", "[TestGetExecutablePath]") {
  std::string executable_path = FileUtils::get_executable_path();
  std::cerr << "Executable path: " << executable_path << std::endl;
  REQUIRE(!executable_path.empty());
}

TEST_CASE("TestFileUtils::get_executable_dir", "[TestGetExecutableDir]") {
  std::string executable_path = FileUtils::get_executable_path();
  std::string executable_dir = FileUtils::get_executable_dir();
  REQUIRE(!executable_dir.empty());
  std::cerr << "Executable dir: " << executable_dir << std::endl;
  REQUIRE(FileUtils::get_parent_path(executable_path) == executable_dir);
}

TEST_CASE("TestFileUtils::create_dir", "[TestCreateDir]") {
  TestController testController;

  auto dir = testController.createTempDirectory();

  std::string test_dir_path = std::string(dir) + FileUtils::get_separator() + "random_dir";

  REQUIRE(FileUtils::create_dir(test_dir_path, false) == 0);  // Dir has to be created successfully
  REQUIRE(std::filesystem::exists(test_dir_path));  // Check if directory exists
  REQUIRE(FileUtils::create_dir(test_dir_path, false) == 0);  // Dir already exists, success should be returned
  REQUIRE(FileUtils::delete_dir(test_dir_path, false) == 0);  // Delete should be successful as well
  test_dir_path += "/random_dir2";
  REQUIRE(FileUtils::create_dir(test_dir_path, false) != 0);  // Create dir should fail for multiple directories if recursive option is not set
}

TEST_CASE("TestFileUtils::create_dir recursively", "[TestCreateDir]") {
  TestController testController;

  auto dir = testController.createTempDirectory();

  std::string test_dir_path = std::string(dir) + FileUtils::get_separator() + "random_dir" + FileUtils::get_separator() +
    "random_dir2" + FileUtils::get_separator() + "random_dir3";

  REQUIRE(FileUtils::create_dir(test_dir_path) == 0);  // Dir has to be created successfully
  REQUIRE(std::filesystem::exists(test_dir_path));  // Check if directory exists
  REQUIRE(FileUtils::create_dir(test_dir_path) == 0);  // Dir already exists, success should be returned
  REQUIRE(FileUtils::delete_dir(test_dir_path) == 0);  // Delete should be successful as well
}

TEST_CASE("TestFileUtils::list_dir", "[TestListDir]") {
  TestController testController;

  struct ListDirLogger {};
  const std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ListDirLogger>::getLogger()};
  LogTestController::getInstance().setDebug<ListDirLogger>();

  // Callback, called for each file entry in the listed directory
  // Return value is used to break (false) or continue (true) listing
  auto lambda = [](const std::string &, const std::string &) -> bool {
    return true;
  };

  auto dir = testController.createTempDirectory();
  auto foo = FileUtils::concat_path(dir, "foo");
  FileUtils::create_dir(foo);

  FileUtils::list_dir(dir, lambda, logger_, false);

  REQUIRE(LogTestController::getInstance().contains(dir));
  REQUIRE_FALSE(LogTestController::getInstance().contains(foo));
}

TEST_CASE("TestFileUtils::list_dir recursively", "[TestListDir]") {
  TestController testController;

  struct ListDirLogger {};
  const std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ListDirLogger>::getLogger()};
  LogTestController::getInstance().setDebug<ListDirLogger>();

  // Callback, called for each file entry in the listed directory
  // Return value is used to break (false) or continue (true) listing
  auto lambda = [](const std::string &, const std::string &) -> bool {
    return true;
  };

  auto dir = testController.createTempDirectory();
  auto foo = FileUtils::concat_path(dir, "foo");
  auto bar = FileUtils::concat_path(dir, "bar");
  auto fooBaz = FileUtils::concat_path(foo, "baz");

  FileUtils::create_dir(foo);
  FileUtils::create_dir(bar);
  FileUtils::create_dir(fooBaz);

  FileUtils::list_dir(dir, lambda, logger_, true);

  REQUIRE(LogTestController::getInstance().contains(dir));
  REQUIRE(LogTestController::getInstance().contains(foo));
  REQUIRE(LogTestController::getInstance().contains(bar));
  REQUIRE(LogTestController::getInstance().contains(fooBaz));
}

TEST_CASE("TestFileUtils::addFilesMatchingExtension", "[TestAddFilesMatchingExtension]") {
  TestController testController;

  struct addFilesMatchingExtension {};
  const std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<addFilesMatchingExtension>::getLogger()};
  LogTestController::getInstance().setInfo<addFilesMatchingExtension>();

  /*dir/
   * |
   * |---foo/
   * |    |
   * |    |--fooFile.ext
   * |    |--fooFile.noext
   * |    |___baz/
   * |
   * |---bar/
   * |    |
   * |    |__barFile.ext
   * |
   * |__level1.ext
   *
   * */

  auto dir = testController.createTempDirectory();
  auto foo = FileUtils::concat_path(dir, "foo");
  auto fooGood = FileUtils::concat_path(foo, "fooFile.ext");
  auto fooBad = FileUtils::concat_path(foo, "fooFile.noext");
  auto fooBaz = FileUtils::concat_path(foo, "baz");

  auto bar = FileUtils::concat_path(dir, "bar");
  auto barGood = FileUtils::concat_path(bar, "barFile.ext");

  auto level1 = FileUtils::concat_path(dir, "level1.ext");

  FileUtils::create_dir(foo);
  FileUtils::create_dir(bar);
  FileUtils::create_dir(fooBaz);
  std::ofstream out1(fooGood);
  std::ofstream out2(fooBad);
  std::ofstream out3(barGood);
  std::ofstream out4(level1);

  std::vector<std::string> expectedFiles = {barGood, fooGood, level1};
  std::vector<std::string> accruedFiles;

  FileUtils::addFilesMatchingExtension(logger_, dir, ".ext", accruedFiles);
  std::sort(accruedFiles.begin(), accruedFiles.end());

  CHECK(accruedFiles == expectedFiles);

  auto fakeDir = FileUtils::concat_path(dir, "fake");
  FileUtils::addFilesMatchingExtension(logger_, fakeDir, ".ext", accruedFiles);
  REQUIRE(LogTestController::getInstance().contains("Failed to open directory: " + fakeDir));
}

TEST_CASE("TestFileUtils::getFullPath", "[TestGetFullPath]") {
  TestController testController;

  const std::string tempDir = utils::file::getFullPath(testController.createTempDirectory());

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
  namespace fs = std::filesystem;

  fs::file_time_type time_before_write = file_clock::now();
  time_point<file_clock, seconds> time_point_before_write = time_point_cast<seconds>(file_clock::now());

  TestController testController;

  std::string dir = testController.createTempDirectory();

  std::string test_file = dir + FileUtils::get_separator() + "test.txt";
  REQUIRE_FALSE(FileUtils::last_write_time(test_file).has_value());  // non existent file should not return last w.t.
  REQUIRE(FileUtils::last_write_time_point(test_file) == (time_point<file_clock, seconds>{}));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::ofstream test_file_stream(test_file);
  test_file_stream << "foo\n";
  test_file_stream.flush();

  fs::file_time_type time_after_first_write = file_clock::now();
  time_point<file_clock, seconds> time_point_after_first_write = time_point_cast<seconds>(file_clock::now());

  fs::file_time_type first_mtime = FileUtils::last_write_time(test_file).value();
  REQUIRE(first_mtime >= time_before_write);
  REQUIRE(first_mtime <= time_after_first_write);

  time_point<file_clock, seconds> first_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(first_mtime_time_point >= time_point_before_write);
  REQUIRE(first_mtime_time_point <= time_point_after_first_write);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  test_file_stream << "bar\n";
  test_file_stream.flush();

  fs::file_time_type time_after_second_write = file_clock::now();
  time_point<file_clock, seconds> time_point_after_second_write = time_point_cast<seconds>(file_clock::now());

  fs::file_time_type second_mtime = FileUtils::last_write_time(test_file).value();
  REQUIRE(second_mtime >= first_mtime);
  REQUIRE(second_mtime >= time_after_first_write);
  REQUIRE(second_mtime <= time_after_second_write);

  time_point<file_clock, seconds> second_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(second_mtime_time_point >= first_mtime_time_point);
  REQUIRE(second_mtime_time_point >= time_point_after_first_write);
  REQUIRE(second_mtime_time_point <= time_point_after_second_write);

  test_file_stream.close();

  // On Windows it would rarely occur that the last_write_time is off by 1 from the previous check
#ifndef WIN32
  fs::file_time_type third_mtime = FileUtils::last_write_time(test_file).value();
  REQUIRE(third_mtime == second_mtime);

  time_point<file_clock, seconds> third_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(third_mtime_time_point == second_mtime_time_point);
#endif
}

TEST_CASE("FileUtils::file_size works", "[file_size]") {
  TestController testController;

  std::string dir = testController.createTempDirectory();

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
  constexpr uint64_t CHECKSUM_OF_0_BYTES = 0U;
  constexpr uint64_t CHECKSUM_OF_4_BYTES = 2117232040U;
  constexpr uint64_t CHECKSUM_OF_11_BYTES = 3461392622U;

  TestController testController;

  std::string dir = testController.createTempDirectory();

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
  constexpr uint64_t CHECKSUM_OF_0_BYTES = 0U;
  constexpr uint64_t CHECKSUM_OF_4095_BYTES = 1902799545U;
  constexpr uint64_t CHECKSUM_OF_4096_BYTES = 1041266625U;
  constexpr uint64_t CHECKSUM_OF_4097_BYTES = 1619129554U;
  constexpr uint64_t CHECKSUM_OF_8192_BYTES = 305726917U;

  TestController testController;

  std::string dir = testController.createTempDirectory();

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
TEST_CASE("FileUtils::set_permissions and get_permissions", "[TestSetPermissions][TestGetPermissions]") {
  TestController testController;

  auto dir = testController.createTempDirectory();
  auto path = dir + FileUtils::get_separator() + "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);

  REQUIRE(FileUtils::set_permissions(path, 0644) == 0);
  uint32_t perms;
  REQUIRE(FileUtils::get_permissions(path, perms));
  REQUIRE(perms == 0644);
}

TEST_CASE("FileUtils::get_permission_string", "[TestGetPermissionString]") {
  TestController testController;

  auto dir = testController.createTempDirectory();
  auto path = dir + FileUtils::get_separator() + "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);

  REQUIRE(FileUtils::set_permissions(path, 0644) == 0);
  auto perms = FileUtils::get_permission_string(path);
  REQUIRE(perms != std::nullopt);
  REQUIRE(*perms == "rw-r--r--");
}
#endif

TEST_CASE("FileUtils::exists", "[TestExists]") {
  TestController testController;

  auto dir = testController.createTempDirectory();
  auto path = dir + FileUtils::get_separator() + "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);
  auto invalid_path = dir + FileUtils::get_separator() + "test_file2.txt";

  REQUIRE(utils::file::exists(path));
  REQUIRE(!utils::file::exists(invalid_path));
}

TEST_CASE("TestFileUtils::delete_dir should fail with empty path", "[TestEmptyDeleteDir]") {
  TestController testController;
  REQUIRE(FileUtils::delete_dir("") != 0);
  REQUIRE(FileUtils::delete_dir("", false) != 0);
}

TEST_CASE("FileUtils::contains", "[utils][file][contains]") {
  TestController test_controller;
  const auto temp_dir = std::filesystem::path{test_controller.createTempDirectory()};

  SECTION("< 8k") {
    const auto file_path = temp_dir / "test_short.txt";
    std::ofstream{file_path} << "This is a test file";
    REQUIRE(utils::file::contains(file_path, "This"));
    REQUIRE(utils::file::contains(file_path, "test"));
    REQUIRE(utils::file::contains(file_path, "file"));
    REQUIRE_FALSE(utils::file::contains(file_path, "hello"));
    REQUIRE_FALSE(utils::file::contains(file_path, "Thiss"));
    REQUIRE_FALSE(utils::file::contains(file_path, "ffile"));
  }
  SECTION("< 16k") {
    const auto file_path = temp_dir / "test_mid.txt";
    {
      std::string contents;
      contents.resize(10240);
      for (size_t i = 0; i < contents.size(); ++i) {
        contents[i] = gsl::narrow<char>('a' + gsl::narrow<int>(i % size_t{'z' - 'a' + 1}));
      }
      const std::string_view src = "12 34 56 Test String";
      contents.replace(8190, src.size(), src);
      std::ofstream ofs{file_path};
      ofs.write(contents.data(), gsl::narrow<std::streamsize>(contents.size()));
    }

    REQUIRE(utils::file::contains(file_path, "xyz"));
    REQUIRE(utils::file::contains(file_path, "12"));
    REQUIRE(utils::file::contains(file_path, " 34"));
    REQUIRE(utils::file::contains(file_path, "12 34"));
    REQUIRE_FALSE(utils::file::contains(file_path, "1234"));
    REQUIRE(utils::file::contains(file_path, "String"));
  }
  SECTION("> 16k") {
    const auto file_path = temp_dir / "test_long.txt";
    std::string buf;
    buf.resize(8192);
    {
      for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = gsl::narrow<char>('A' + gsl::narrow<int>(i % size_t{'Z' - 'A' + 1}));
      }
      std::ofstream ofs{file_path};
      ofs.write(buf.data(), gsl::narrow<std::streamsize>(buf.size()));
      ofs << " apple banana orange 1234 ";
      ofs.write(buf.data(), gsl::narrow<std::streamsize>(buf.size()));
    }

    REQUIRE(utils::file::contains(file_path, std::string_view{buf.data(), buf.size()}));
    std::rotate(std::begin(buf), std::next(std::begin(buf), 6), std::end(buf));
    buf.replace(8192 - 6, 6, " apple", 6);
    REQUIRE(utils::file::contains(file_path, std::string_view{buf.data(), buf.size()}));
    buf.replace(8192 - 6, 6, "banana", 6);
    REQUIRE_FALSE(utils::file::contains(file_path, std::string_view{buf.data(), buf.size()}));

    REQUIRE(utils::file::contains(file_path, "apple"));
    REQUIRE(utils::file::contains(file_path, "banana"));
    REQUIRE(utils::file::contains(file_path, "ABC"));
  }
}

TEST_CASE("FileUtils::get_relative_path", "[TestGetRelativePath]") {
  TestController test_controller;
  const auto base_path = test_controller.createTempDirectory();
  auto path = std::filesystem::path{"/random/non-existent/dir"};
  REQUIRE(FileUtils::get_relative_path(path.string(), base_path) == std::nullopt);
  path = std::filesystem::path{base_path} / "subdir" / "file.log";
  REQUIRE(*FileUtils::get_relative_path(path.string(), base_path) == std::string("subdir") + FileUtils::get_separator() + "file.log");
  REQUIRE(*FileUtils::get_relative_path(path.string(), base_path + FileUtils::get_separator()) == std::string("subdir") + FileUtils::get_separator() + "file.log");
  REQUIRE(*FileUtils::get_relative_path(base_path, base_path) == ".");
}
