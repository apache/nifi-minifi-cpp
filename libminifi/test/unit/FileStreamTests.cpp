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

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "io/FileStream.h"
#include "../TestBase.h"
#include "utils/gsl.h"

#ifdef USE_BOOST
#include <boost/filesystem.hpp>
#endif

TEST_CASE("TestFileOverWrite", "[TestFiles]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.read(readBuffer, gsl::narrow<int>(stream.size())) == stream.size());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(reinterpret_cast<uint8_t*>(const_cast<char*>("file")), 4);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.read(verifybuffer, gsl::narrow<int>(stream.size())) == stream.size());

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempfile");

  std::remove(ss.str().c_str());
}

TEST_CASE("TestFileBadArgumentNoChange", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.read(readBuffer, gsl::narrow<int>(stream.size())) == stream.size());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(reinterpret_cast<uint8_t*>(const_cast<char*>("file")), 0);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.read(verifybuffer, gsl::narrow<int>(stream.size())) == stream.size());

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempFile");

  std::remove(ss.str().c_str());
}

TEST_CASE("TestFileBadArgumentNoChange2", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.read(readBuffer, gsl::narrow<int>(stream.size())) == stream.size());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(nullptr, 0);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.read(verifybuffer, gsl::narrow<int>(stream.size())) == stream.size());

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempFile");

  std::remove(ss.str().c_str());
}

TEST_CASE("TestFileBadArgumentNoChange3", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.read(readBuffer, gsl::narrow<int>(stream.size())) == stream.size());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(4);

  stream.write(nullptr, 0);

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.read(nullptr, gsl::narrow<int>(stream.size())) == -1);

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "");

  std::remove(ss.str().c_str());
}

TEST_CASE("TestFileBeyondEnd3", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.read(readBuffer, gsl::narrow<int>(stream.size())) == stream.size());

  uint8_t* data = readBuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), readBuffer.size()) == "tempFile");

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  REQUIRE(stream.read(verifybuffer, 8192) == 8);

  data = verifybuffer.data();

  REQUIRE(std::string(reinterpret_cast<char*>(data), verifybuffer.size()) == "tempFile");

  std::remove(ss.str().c_str());
}

TEST_CASE("TestFileExceedSize", "[TestLoader]") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  std::string path = ss.str();
  file.open(path, std::ios::out);
  for (int i = 0; i < 10240; i++)
    file << "tempFile";
  file.close();

  minifi::io::FileStream stream(path, 0, true);
  std::vector<uint8_t> readBuffer;
  REQUIRE(stream.read(readBuffer, gsl::narrow<int>(stream.size())) == stream.size());

  uint8_t* data = readBuffer.data();

  stream.seek(0);

  std::vector<uint8_t> verifybuffer;

  for (int i = 0; i < 10; i++) {
    REQUIRE(stream.read(verifybuffer, 8192) == 8192);
  }
  REQUIRE(stream.read(verifybuffer, 8192) == 0);
  stream.seek(0);
  for (int i = 0; i < 10; i++) {
    REQUIRE(stream.read(verifybuffer, 8192) == 8192);
  }
  REQUIRE(stream.read(verifybuffer, 8192) == 0);

  std::remove(ss.str().c_str());
}

TEST_CASE("Write zero bytes") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  minifi::io::FileStream stream(utils::file::concat_path(dir, "test.txt"), 0, true);
  REQUIRE(stream.write(nullptr, 0) == 0);
}

TEST_CASE("Read zero bytes") {
  TestController testController;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);
  minifi::io::FileStream stream(utils::file::concat_path(dir, "test.txt"), 0, true);
  REQUIRE(stream.read(nullptr, 0) == 0);
}

TEST_CASE("Non-existing file read/write test") {
  TestController test_controller;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = test_controller.createTempDirectory(format);
  minifi::io::FileStream stream(utils::file::concat_path(dir, "non_existing_file.txt"), 0, true);
  REQUIRE(test_controller.getLog().getInstance().contains("Error opening file", std::chrono::seconds(0)));
  REQUIRE(test_controller.getLog().getInstance().contains("No such file or directory", std::chrono::seconds(0)));
  REQUIRE(stream.write("lorem ipsum", false) == -1);
  REQUIRE(test_controller.getLog().getInstance().contains("Error writing to file: invalid file stream", std::chrono::seconds(0)));
  std::vector<uint8_t> readBuffer;
  stream.seek(0);
  REQUIRE(stream.read(readBuffer, 1) == -1);
  REQUIRE(test_controller.getLog().getInstance().contains("Error reading from file: invalid file stream", std::chrono::seconds(0)));
}

TEST_CASE("Existing file read/write test") {
  TestController test_controller;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = test_controller.createTempDirectory(format);
  std::string path_to_existing_file(utils::file::concat_path(dir, "existing_file.txt"));
  {
    std::ofstream outfile(path_to_existing_file);
    outfile << "lorem ipsum" << std::endl;
    outfile.close();
  }
  minifi::io::FileStream stream(path_to_existing_file, 0, true);
  REQUIRE_FALSE(test_controller.getLog().getInstance().contains("Error opening file", std::chrono::seconds(0)));
  REQUIRE_FALSE(stream.write("dolor sit amet", false) == -1);
  REQUIRE_FALSE(test_controller.getLog().getInstance().contains("Error writing to file", std::chrono::seconds(0)));
  std::vector<uint8_t> readBuffer;
  stream.seek(0);
  REQUIRE_FALSE(stream.read(readBuffer, 11) == -1);
  REQUIRE_FALSE(test_controller.getLog().getInstance().contains("Error reading from file", std::chrono::seconds(0)));
  stream.seek(0);
  REQUIRE(stream.read(nullptr, 11) == -1);
  REQUIRE(test_controller.getLog().getInstance().contains("Error reading from file: invalid buffer", std::chrono::seconds(0)));
}

#ifdef USE_BOOST
TEST_CASE("Opening file without permission creates error logs") {
  TestController test_controller;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = test_controller.createTempDirectory(format);
  std::string path_to_permissionless_file(utils::file::concat_path(dir, "permissionless_file.txt"));
  {
    std::ofstream outfile(path_to_permissionless_file);
    outfile << "this file has been just created" << std::endl;
    outfile.close();
    // This could be done with C++17 std::filesystem
    boost::filesystem::permissions(path_to_permissionless_file, boost::filesystem::no_perms);
  }
  minifi::io::FileStream stream(path_to_permissionless_file, 0, false);
  REQUIRE(test_controller.getLog().getInstance().contains("Error opening file", std::chrono::seconds(0)));
  REQUIRE(test_controller.getLog().getInstance().contains("Permission denied", std::chrono::seconds(0)));
}
#endif

TEST_CASE("Readonly filestream write test") {
  TestController test_controller;
  char format[] = "/tmp/gt.XXXXXX";
  auto dir = test_controller.createTempDirectory(format);
  std::string path_to_file(utils::file::concat_path(dir, "file_to_seek_in.txt"));
  {
    std::ofstream outfile(path_to_file);
    outfile << "lorem ipsum" << std::endl;
    outfile.close();
  }
  minifi::io::FileStream stream(path_to_file, 0, false);
  REQUIRE(stream.write("dolor sit amet", false) == -1);
  REQUIRE(test_controller.getLog().getInstance().contains("Error writing to file: write call on file stream failed", std::chrono::seconds(0)));
}
