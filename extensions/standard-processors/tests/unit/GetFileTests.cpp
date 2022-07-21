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
#include <utility>
#include <memory>
#include <string>
#include <fstream>
#include <chrono>

#include "TestBase.h"
#include "Catch.h"
#include "LogAttribute.h"
#include "GetFile.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"
#include "unit/ProvenanceTestHelper.h"

#ifdef WIN32
#include <fileapi.h>
#endif

using namespace std::literals::chrono_literals;

namespace {

class GetFileTestController {
 public:
  GetFileTestController();
  [[nodiscard]] std::string getFullPath(const std::string& filename) const;
  [[nodiscard]] std::string getInputFilePath() const;
  void setProperty(const core::Property& property, const std::string& value);
  void runSession();
  void resetTestPlan();

 private:
  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan_;
  std::string temp_dir_;
  std::string input_file_name_;
  std::string large_input_file_name_;
  std::string hidden_input_file_name_;
  std::shared_ptr<core::Processor> get_file_processor_;
};

GetFileTestController::GetFileTestController()
  : test_plan_(test_controller_.createPlan()),
    temp_dir_(test_controller_.createTempDirectory()),
    input_file_name_("test.txt"),
    large_input_file_name_("large_file.txt"),
    hidden_input_file_name_(".test.txt") {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  REQUIRE(!temp_dir_.empty());

  // Build MiNiFi processing graph
  get_file_processor_ = test_plan_->addProcessor("GetFile", "Get");
  test_plan_->setProperty(get_file_processor_, minifi::processors::GetFile::Directory.getName(), temp_dir_);
  auto log_attr = test_plan_->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  test_plan_->setProperty(log_attr, minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

  utils::putFileToDir(temp_dir_, input_file_name_, "The quick brown fox jumps over the lazy dog\n");
  utils::putFileToDir(temp_dir_, large_input_file_name_, "The quick brown fox jumps over the lazy dog who is 2 legit to quit\n");
  utils::putFileToDir(temp_dir_, hidden_input_file_name_, "But noone has ever seen it\n");

#ifdef WIN32
  const auto hide_file_err = utils::file::FileUtils::hide_file(getFullPath(hidden_input_file_name_).c_str());
  REQUIRE(!hide_file_err);
#endif
}

std::string GetFileTestController::getFullPath(const std::string& filename) const {
  return temp_dir_ + utils::file::FileUtils::get_separator() + filename;
}

std::string GetFileTestController::getInputFilePath() const {
  return getFullPath(input_file_name_);
}

void GetFileTestController::setProperty(const core::Property& property, const std::string& value) {
  test_plan_->setProperty(get_file_processor_, property.getName(), value);
}

void GetFileTestController::runSession() {
  test_controller_.runSession(test_plan_);
}

void GetFileTestController::resetTestPlan() {
  test_plan_->reset();
}

}  // namespace

TEST_CASE("GetFile ignores hidden files and files larger than MaxSize", "[GetFile]") {
  GetFileTestController test_controller;
  SECTION("IgnoreHiddenFile not set, so defaults to true") {}
  SECTION("IgnoreHiddenFile set to true explicitly") { test_controller.setProperty(minifi::processors::GetFile::IgnoreHiddenFile, "true"); }
  test_controller.setProperty(minifi::processors::GetFile::MaxSize, "50 B");

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));  // The hidden and the too big files should be ignored
  REQUIRE(LogTestController::getInstance().contains("key:filename value:test.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:flow.id"));
  REQUIRE(LogTestController::getInstance().contains("Size:44 Offset:0"));
}

TEST_CASE("GetFile ignores files smaller than MinSize", "[GetFile]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::MinSize, "50 B");

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:large_file.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:flow.id"));
  REQUIRE(LogTestController::getInstance().contains("Size:67 Offset:0"));
}

TEST_CASE("GetFile onSchedule() throws if the required Directory property is not set", "[GetFile]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::Directory, "");
  REQUIRE_THROWS_AS(test_controller.runSession(), minifi::Exception);
}

TEST_CASE("GetFile removes the source file if KeepSourceFile is false") {
  GetFileTestController test_controller;
  SECTION("KeepSourceFile is not set, so defaults to false") {}
  SECTION("KeepSourceFile is set to false explicitly") { test_controller.setProperty(minifi::processors::GetFile::KeepSourceFile, "false"); }

  test_controller.runSession();

  REQUIRE_FALSE(utils::file::exists(test_controller.getInputFilePath()));
}

TEST_CASE("GetFile keeps the source file if KeepSourceFile is true") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::KeepSourceFile, "true");

  test_controller.runSession();

  REQUIRE(utils::file::exists(test_controller.getInputFilePath()));
}

TEST_CASE("Hidden files are read when IgnoreHiddenFile property is false", "[getFileProperty]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::IgnoreHiddenFile, "false");

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:large_file.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:test.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:.test.txt"));
}

TEST_CASE("Check if subdirectories are ignored or not if Recurse property is set", "[getFileProperty]") {
  GetFileTestController test_controller;

  auto subdir_path = test_controller.getFullPath("subdir");
  utils::file::FileUtils::create_dir(subdir_path);
  utils::putFileToDir(subdir_path, "subfile.txt", "Some content in a subfile\n");

  SECTION("File in subdirectory is ignored when Recurse property set to false")  {
    test_controller.setProperty(minifi::processors::GetFile::Recurse, "false");
    test_controller.runSession();

    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:test.txt"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:large_file.txt"));
  }

  SECTION("File in subdirectory is logged when Recurse property set to true")  {
    test_controller.setProperty(minifi::processors::GetFile::Recurse, "true");
    test_controller.runSession();

    REQUIRE(LogTestController::getInstance().contains("Logged 3 flow files"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:test.txt"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:large_file.txt"));
    REQUIRE(LogTestController::getInstance().contains("key:filename value:subfile.txt"));
  }
}

TEST_CASE("Only older files are read when MinAge property is set", "[getFileProperty]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::MinAge, "1 hour");

  const auto more_than_an_hour_ago = std::chrono::file_clock::now() - 65min;
  utils::file::FileUtils::set_last_write_time(test_controller.getInputFilePath(), more_than_an_hour_ago);

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:test.txt"));
  REQUIRE(LogTestController::getInstance().contains("Size:44 Offset:0"));
}

TEST_CASE("Only newer files are read when MaxAge property is set", "[getFileProperty]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::MaxAge, "1 hour");

  const auto more_than_an_hour_ago = std::chrono::file_clock::now() - 65min;
  utils::file::FileUtils::set_last_write_time(test_controller.getInputFilePath(), more_than_an_hour_ago);

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:large_file.txt"));
  REQUIRE(LogTestController::getInstance().contains("Size:67 Offset:0"));
}

TEST_CASE("Test BatchSize property for the maximum number of files read at once", "[getFileProperty]") {
  GetFileTestController test_controller;

  SECTION("BatchSize is set to 1 so only 1 file should be logged")  {
    test_controller.setProperty(minifi::processors::GetFile::BatchSize, "1");
    test_controller.runSession();
    REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));
  }

  SECTION("BatchSize is set to 5 so all 2 non-hidden files should be logged")  {
    test_controller.setProperty(minifi::processors::GetFile::BatchSize, "5");
    test_controller.runSession();
    REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  }
}

TEST_CASE("Test file filtering of GetFile", "[getFileProperty]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::FileFilter, ".?test\\.txt$");
  test_controller.setProperty(minifi::processors::GetFile::IgnoreHiddenFile, "false");

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:test.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:.test.txt"));
}

TEST_CASE("Test if GetFile honors PollInterval property when triggered multiple times between intervals", "[getFileProperty]") {
  GetFileTestController test_controller;
  test_controller.setProperty(minifi::processors::GetFile::PollInterval, "100 ms");
  test_controller.setProperty(minifi::processors::GetFile::KeepSourceFile, "true");

  auto start_time = std::chrono::steady_clock::now();
  test_controller.runSession();
  while (LogTestController::getInstance().countOccurrences("Logged 2 flow files") < 2) {
    test_controller.resetTestPlan();
    test_controller.runSession();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  REQUIRE(std::chrono::steady_clock::now() - start_time >= 100ms);
}
