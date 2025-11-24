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

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "utils/file/FileUtils.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/TimeUtil.h"

using namespace std::literals::chrono_literals;

namespace FileUtils = org::apache::nifi::minifi::utils::file;

TEST_CASE("TestFileUtils::get_executable_path", "[TestGetExecutablePath]") {
  auto executable_path = FileUtils::get_executable_path();
  std::cerr << "Executable path: " << executable_path << std::endl;
  REQUIRE(!executable_path.empty());
}

TEST_CASE("TestFileUtils::get_executable_dir", "[TestGetExecutableDir]") {
  auto executable_path = FileUtils::get_executable_path();
  auto executable_dir = FileUtils::get_executable_dir();
  REQUIRE(!executable_dir.empty());
  std::cerr << "Executable dir: " << executable_dir << std::endl;
  REQUIRE(executable_path.parent_path() == executable_dir);
}

TEST_CASE("TestFileUtils::create_dir", "[TestCreateDir]") {
  TestController testController;

  auto dir = testController.createTempDirectory();

  auto test_dir_path = dir / "random_dir";

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

  auto test_dir_path = dir / "random_dir" / "random_dir2" / "random_dir3";

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
  auto lambda = [](const std::filesystem::path&, const std::filesystem::path&) -> bool {
    return true;
  };

  auto dir = testController.createTempDirectory();
  auto foo = dir / "foo";
  FileUtils::create_dir(foo);

  FileUtils::list_dir(dir, lambda, logger_, false);

  REQUIRE(LogTestController::getInstance().contains(dir.string()));
  REQUIRE_FALSE(LogTestController::getInstance().contains(foo.string()));
}

TEST_CASE("TestFileUtils::list_dir recursively", "[TestListDir]") {
  TestController testController;

  struct ListDirLogger {};
  const std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ListDirLogger>::getLogger()};
  LogTestController::getInstance().setDebug<ListDirLogger>();

  // Callback, called for each file entry in the listed directory
  // Return value is used to break (false) or continue (true) listing
  auto lambda = [](const std::filesystem::path&, const std::filesystem::path&) -> bool {
    return true;
  };

  auto dir = testController.createTempDirectory();
  auto foo = dir / "foo";
  auto bar = dir / "bar";
  auto fooBaz = foo / "baz";

  FileUtils::create_dir(foo);
  FileUtils::create_dir(bar);
  FileUtils::create_dir(fooBaz);

  FileUtils::list_dir(dir, lambda, logger_, true);

  REQUIRE(LogTestController::getInstance().contains(dir.string()));
  REQUIRE(LogTestController::getInstance().contains(foo.string()));
  REQUIRE(LogTestController::getInstance().contains(bar.string()));
  REQUIRE(LogTestController::getInstance().contains(fooBaz.string()));
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
  auto foo = dir / "foo";
  auto fooGood = foo / "fooFile.ext";
  auto fooBad = foo / "fooFile.noext";
  auto fooBaz = foo / "baz";

  auto bar = dir / "bar";
  auto barGood = bar / "barFile.ext";

  auto level1 = dir / "level1.ext";

  FileUtils::create_dir(foo);
  FileUtils::create_dir(bar);
  FileUtils::create_dir(fooBaz);
  std::ofstream out1(fooGood);
  std::ofstream out2(fooBad);
  std::ofstream out3(barGood);
  std::ofstream out4(level1);

  std::vector<std::filesystem::path> expectedFiles = {barGood, fooGood, level1};
  std::vector<std::filesystem::path> accruedFiles;

  FileUtils::addFilesMatchingExtension(logger_, dir, ".ext", accruedFiles);
  std::sort(accruedFiles.begin(), accruedFiles.end());

  CHECK(accruedFiles == expectedFiles);

  auto fakeDir = dir / "fake";
  FileUtils::addFilesMatchingExtension(logger_, fakeDir, ".ext", accruedFiles);
  REQUIRE(LogTestController::getInstance().contains("Failed to open directory: " + fakeDir.string()));
}

TEST_CASE("FileUtils::last_write_time and last_write_time_point work", "[last_write_time][last_write_time_point]") {
  auto time_before_write = std::chrono::file_clock::now();
  auto time_point_before_write = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::file_clock::now());

  TestController testController;

  auto dir = testController.createTempDirectory();

  auto test_file = dir / "test.txt";
  REQUIRE_FALSE(FileUtils::last_write_time(test_file).has_value());  // non existent file should not return last w.t.
  REQUIRE(FileUtils::last_write_time_point(test_file) == (std::chrono::time_point<std::chrono::file_clock, std::chrono::seconds>{}));

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::ofstream test_file_stream(test_file);
  test_file_stream << "foo\n";
  test_file_stream.flush();

  auto time_after_first_write = std::chrono::file_clock::now();
  auto time_point_after_first_write = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::file_clock::now());

  auto first_mtime = FileUtils::last_write_time(test_file).value();
  REQUIRE(first_mtime >= time_before_write);
  REQUIRE(first_mtime <= time_after_first_write);

  auto first_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(first_mtime_time_point >= time_point_before_write);
  REQUIRE(first_mtime_time_point <= time_point_after_first_write);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  test_file_stream << "bar\n";
  test_file_stream.flush();

  auto time_after_second_write = std::chrono::file_clock::now();
  auto time_point_after_second_write = time_point_cast<std::chrono::seconds>(std::chrono::file_clock::now());

  auto second_mtime = FileUtils::last_write_time(test_file).value();
  REQUIRE(second_mtime >= first_mtime);
  REQUIRE(second_mtime >= time_after_first_write);
  REQUIRE(second_mtime <= time_after_second_write);

  auto second_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(second_mtime_time_point >= first_mtime_time_point);
  REQUIRE(second_mtime_time_point >= time_point_after_first_write);
  REQUIRE(second_mtime_time_point <= time_point_after_second_write);

  test_file_stream.close();

  // On Windows it would rarely occur that the last_write_time is off by 1 from the previous check
#ifndef WIN32
  auto third_mtime = FileUtils::last_write_time(test_file).value();
  REQUIRE(third_mtime == second_mtime);

  auto third_mtime_time_point = FileUtils::last_write_time_point(test_file);
  REQUIRE(third_mtime_time_point == second_mtime_time_point);
#endif
}

TEST_CASE("FileUtils::file_size works", "[file_size]") {
  TestController testController;

  auto dir = testController.createTempDirectory();

  auto test_file = dir / "test.txt";
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

  auto dir = testController.createTempDirectory();

  auto test_file = dir / "test.txt";
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


  auto another_file = dir / "another_test.txt";
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

  auto dir = testController.createTempDirectory();

  auto test_file = dir / "test.txt";
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


  auto another_file = dir / "another_test.txt";
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
  auto path = dir / "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);

  REQUIRE(FileUtils::set_permissions(path, 0644) == 0);
  uint32_t perms = 0;
  REQUIRE(FileUtils::get_permissions(path, perms));
  REQUIRE(perms == 0644);
}

TEST_CASE("FileUtils::get_permission_string", "[TestGetPermissionString]") {
  TestController testController;

  auto dir = testController.createTempDirectory();
  auto path = dir / "test_file.txt";
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
  auto path = dir / "test_file.txt";
  std::ofstream outfile(path, std::ios::out | std::ios::binary);
  auto invalid_path = dir / "test_file2.txt";

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
  REQUIRE(FileUtils::get_relative_path(path, base_path) == std::nullopt);
  path = std::filesystem::path{base_path} / "subdir" / "file.log";
  REQUIRE(*FileUtils::get_relative_path(path, base_path) == std::filesystem::path("subdir") / "file.log");
  REQUIRE(*FileUtils::get_relative_path(path, base_path / "") == std::filesystem::path("subdir") / "file.log");
  REQUIRE(*FileUtils::get_relative_path(base_path, base_path) == ".");
}

TEST_CASE("FileUtils::path_size", "[TestPathSize]") {
  auto writeToFile = [](const std::filesystem::path& path) {
    std::ofstream test_file_stream(path, std::ios::out | std::ios::binary);
    test_file_stream << "foo\n";
    test_file_stream.flush();
  };

  TestController test_controller;
  REQUIRE(FileUtils::path_size({""}) == 0);
  REQUIRE(FileUtils::path_size({"/random/non-existent/dir"}) == 0);

  auto dir = test_controller.createTempDirectory();
  REQUIRE(FileUtils::path_size(dir) == 0);

  auto test_file = dir / "test_file.log";
  writeToFile(test_file);

  REQUIRE(FileUtils::path_size(test_file) == 4);
  REQUIRE(FileUtils::path_size(dir) == 4);

  auto subdir = dir / "subdir";
  REQUIRE(utils::file::create_dir(subdir) == 0);

  REQUIRE(FileUtils::path_size(dir) == 4);

  auto subdir_test_file = subdir / "test_file2.log";
  writeToFile(subdir_test_file);

  REQUIRE(FileUtils::path_size(dir) == 8);

  auto subsubdir = subdir / "subsubdir";
  REQUIRE(utils::file::create_dir(subsubdir) == 0);

  auto subsubdir_test_file = subsubdir / "test_file3.log";
  writeToFile(subsubdir_test_file);

  REQUIRE(FileUtils::path_size(dir) == 12);
}

TEST_CASE("file_clock to system_clock conversion tests") {
  static_assert(std::chrono::system_clock::period::num == std::chrono::file_clock::period::num);
  constexpr auto lowest_den = std::min(std::chrono::file_clock::period::den, std::chrono::system_clock::period::den);
  using LeastPreciseDurationType = std::chrono::duration<std::common_type_t<std::chrono::system_clock::duration::rep, std::chrono::file_clock::duration::rep>,
    std::ratio<std::chrono::system_clock::period::num, lowest_den>>;

  {
    std::chrono::system_clock::time_point system_now = std::chrono::system_clock::now();
    std::chrono::file_clock::time_point converted_system_now = FileUtils::from_sys(system_now);
    std::chrono::system_clock::time_point double_converted_system_now = FileUtils::to_sys(converted_system_now);

    CHECK(std::chrono::time_point_cast<LeastPreciseDurationType>(system_now).time_since_epoch().count() ==
      std::chrono::time_point_cast<LeastPreciseDurationType>(double_converted_system_now).time_since_epoch().count());
  }

  {
    std::chrono::file_clock::time_point file_now = std::chrono::file_clock ::now();
    std::chrono::system_clock::time_point converted_file_now = FileUtils::to_sys(file_now);
    std::chrono::file_clock::time_point double_converted_file_now = FileUtils::from_sys(converted_file_now);

    CHECK(std::chrono::time_point_cast<LeastPreciseDurationType>(file_now).time_since_epoch().count() ==
      std::chrono::time_point_cast<LeastPreciseDurationType>(double_converted_file_now).time_since_epoch().count());
  }

  {
    // t0 <= t1
    auto sys_time_t0 = std::chrono::system_clock::now();
    auto file_time_t1 = std::chrono::file_clock ::now();

    auto file_time_from_t0 = FileUtils::from_sys(sys_time_t0);
    auto sys_time_from_t1 = FileUtils::to_sys(file_time_t1);

    CHECK(0ms <= sys_time_from_t1-sys_time_t0);
    CHECK(sys_time_from_t1-sys_time_t0 < 10ms);

    CHECK(0ms <= file_time_t1-file_time_from_t0);
    CHECK(file_time_t1-file_time_from_t0 < 10ms);
  }
}

TEST_CASE("FileUtils::findSubstringWithPrefix", "[TestFindSubstringWithPrefix]") {
  std::stringstream ss;
  ss << "garbage tag=1.2.3garbage tag=5.6.7";
  SECTION("Exists") {
    CHECK(utils::file::findSubstringWithPrefix(ss, "tag=", 9) == "tag=1.2.3");
  }
  SECTION("Does not exist") {
    CHECK(utils::file::findSubstringWithPrefix(ss, "notag=", 9) == std::nullopt);
  }
}
