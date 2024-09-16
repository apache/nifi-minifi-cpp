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
#include <memory>
#include <string>

#include "unit/TestBase.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/Catch.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "processors/LogAttribute.h"
#include "processors/ListFile.h"
#include "unit/TestUtils.h"
#include "utils/file/PathUtils.h"

using namespace std::literals::chrono_literals;

namespace {

#ifdef WIN32
inline char get_separator() {
  return '\\';
}
#else
inline char get_separator() {
  return '/';
}
#endif

using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;

class ListFileTestFixture {
 public:
  ListFileTestFixture();

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  const std::filesystem::path input_dir_;
  core::Processor* list_file_processor_;
  std::filesystem::path hidden_file_path_;
  std::filesystem::path empty_file_abs_path_;
  std::filesystem::path standard_file_abs_path_;
  std::filesystem::path first_sub_file_abs_path_;
  std::filesystem::path second_sub_file_abs_path_;
};

ListFileTestFixture::ListFileTestFixture()
  : plan_(test_controller_.createPlan()),
    input_dir_(test_controller_.createTempDirectory()) {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::ListFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  REQUIRE(!input_dir_.empty());

  list_file_processor_ = plan_->addProcessor("ListFile", "ListFile");
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::InputDirectory, input_dir_.string());
  auto log_attribute = plan_->addProcessor("LogAttribute", "logAttribute", core::Relationship("success", "description"), true);
  plan_->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

  hidden_file_path_ = minifi::test::utils::putFileToDir(input_dir_, ".hidden_file.txt", "hidden");
  standard_file_abs_path_ = minifi::test::utils::putFileToDir(input_dir_, "standard_file.log", "test");
  empty_file_abs_path_ = minifi::test::utils::putFileToDir(input_dir_, "empty_file.txt", "");
  utils::file::FileUtils::create_dir(input_dir_ / "first_subdir");
  first_sub_file_abs_path_ = minifi::test::utils::putFileToDir(input_dir_ / "first_subdir", "sub_file_one.txt", "the");
  utils::file::FileUtils::create_dir(input_dir_ / "second_subdir");
  second_sub_file_abs_path_ = minifi::test::utils::putFileToDir(input_dir_ / "second_subdir", "sub_file_two.txt", "some_other_content");

  auto last_write_time = *utils::file::FileUtils::last_write_time(standard_file_abs_path_);
  utils::file::FileUtils::set_last_write_time(empty_file_abs_path_, last_write_time - 1h);
  utils::file::FileUtils::set_last_write_time(first_sub_file_abs_path_, last_write_time - 2h);
  utils::file::FileUtils::set_last_write_time(second_sub_file_abs_path_, last_write_time - 3h);
#ifndef WIN32
  REQUIRE(0 == utils::file::FileUtils::set_permissions(input_dir_ / "empty_file.txt", 0755));
  REQUIRE(0 == utils::file::FileUtils::set_permissions(input_dir_ / "standard_file.log", 0644));
#endif

#ifdef WIN32
  const auto hide_file_error = minifi::test::utils::hide_file(hidden_file_path_.c_str());
  REQUIRE(!hide_file_error);
#endif
}

TEST_CASE_METHOD(ListFileTestFixture, "Input Directory is empty", "[testListFile]") {
  CHECK(list_file_processor_->clearProperty(minifi::processors::ListFile::InputDirectory.name));
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), std::runtime_error);
}

std::string get_last_modified_time_formatted_string(const std::filesystem::path& path) {
  return utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(utils::file::to_sys(*utils::file::last_write_time(path))));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files only once with default parameters", "[testListFile]") {
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));

  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + (empty_file_abs_path_.parent_path() / "").string() + "\n"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + (standard_file_abs_path_.parent_path() / "").string() + "\n"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + (first_sub_file_abs_path_.parent_path() / "").string() + "\n"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + (second_sub_file_abs_path_.parent_path() / "").string() + "\n"));

  REQUIRE(LogTestController::getInstance().countOccurrences(std::string("key:path value:.") + get_separator() + "\n") == 2);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, std::string("key:path value:first_subdir") + get_separator()));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, std::string("key:path value:second_subdir") + get_separator()));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:.hidden_file.txt") == 0);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.size value:0"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.size value:4"));
#ifndef WIN32
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.permissions value:rwxr-xr-x"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.permissions value:rw-r--r--"));
  if (auto group = utils::file::FileUtils::get_file_group(standard_file_abs_path_)) {
    REQUIRE(LogTestController::getInstance().countOccurrences("key:file.group value:" + *group) == 4);
  }
#endif
  if (auto owner = utils::file::FileUtils::get_file_owner(standard_file_abs_path_)) {
    REQUIRE(LogTestController::getInstance().countOccurrences("key:file.owner value:" + *owner) == 4);
  }
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + get_last_modified_time_formatted_string(empty_file_abs_path_)));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + get_last_modified_time_formatted_string(standard_file_abs_path_)));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + get_last_modified_time_formatted_string(first_sub_file_abs_path_)));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + get_last_modified_time_formatted_string(second_sub_file_abs_path_)));
  plan_->reset();
  LogTestController::getInstance().clear();
  TestController::runSession(plan_, true);
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:file.size", 0s, 0ms));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test turning off recursive file listing", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::RecurseSubdirectories, "false");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_one.txt", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_two.txt", 0s, 0ms));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files matching the File Filter pattern", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::FileFilter, "stand\\w+\\.log");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:empty_file.txt", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_one.txt", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_two.txt", 0s, 0ms));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files matching the Path Filter pattern", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::PathFilter, "first.*");
  TestController::runSession(plan_);
  CHECK(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  CHECK(LogTestController::getInstance().countOccurrences("key:filename value:") == 1);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files matching the Path Filter pattern when the pattern also matches .", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::PathFilter, "second.*|\\.");
  TestController::runSession(plan_);
  CHECK(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  CHECK(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  CHECK(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  CHECK(LogTestController::getInstance().countOccurrences("key:filename value:") == 3);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the minimum file age", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::MinimumFileAge, "90 min");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:empty_file.txt") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:standard_file.log") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the maximum file age", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::MaximumFileAge, "90 min");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_one.txt") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_two.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the minimum file size", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::MinimumFileSize, "4 B");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:empty_file.txt") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_one.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the maximum file size", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::MaximumFileSize, "4 B");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_two.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing hidden files", "[testListFile]") {
  plan_->setProperty(list_file_processor_, minifi::processors::ListFile::IgnoreHiddenFiles, "false");
  TestController::runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:.hidden_file.txt"));
}

TEST_CASE("ListFile sets attributes correctly") {
  using minifi::processors::ListFile;

  LogTestController::getInstance().setTrace<ListFile>();
  minifi::test::SingleProcessorTestController test_controller(std::make_unique<ListFile>("ListFile"));
  const auto list_file = test_controller.getProcessor();
  std::filesystem::path dir = test_controller.createTempDirectory();
  list_file->setProperty(ListFile::InputDirectory.name, dir.string());
  SECTION("File in subdirectory of input directory") {
    std::filesystem::create_directories(dir / "a" / "b");
    minifi::test::utils::putFileToDir(dir / "a" / "b", "alpha.txt", "The quick brown fox jumps over the lazy dog\n");
    auto result = test_controller.trigger();
    REQUIRE((result.contains(ListFile::Success) && result.at(ListFile::Success).size() == 1));
    auto flow_file = result.at(ListFile::Success)[0];
    CHECK(flow_file->getAttribute(minifi::core::SpecialFlowAttribute::PATH) == (std::filesystem::path("a") / "b" / "").string());
    CHECK(flow_file->getAttribute(minifi::core::SpecialFlowAttribute::ABSOLUTE_PATH) == (dir / "a" / "b" / "").string());
    CHECK(flow_file->getAttribute(minifi::core::SpecialFlowAttribute::FILENAME) == "alpha.txt");
  }
  SECTION("File directly in input directory") {
    minifi::test::utils::putFileToDir(dir, "beta.txt", "The quick brown fox jumps over the lazy dog\n");
    auto result = test_controller.trigger();
    REQUIRE((result.contains(ListFile::Success) && result.at(ListFile::Success).size() == 1));
    auto flow_file = result.at(ListFile::Success)[0];
    CHECK(flow_file->getAttribute(minifi::core::SpecialFlowAttribute::PATH) == (std::filesystem::path(".") / "").string());
    CHECK(flow_file->getAttribute(minifi::core::SpecialFlowAttribute::ABSOLUTE_PATH) == (dir / "").string());
    CHECK(flow_file->getAttribute(minifi::core::SpecialFlowAttribute::FILENAME) == "beta.txt");
  }
}

TEST_CASE("If a second file with the same modification time shows up later, then it will get listed") {
  using minifi::processors::ListFile;

  minifi::test::SingleProcessorTestController test_controller(std::make_unique<ListFile>("ListFile"));
  const auto list_file = test_controller.getProcessor();

  const auto input_dir = test_controller.createTempDirectory();
  list_file->setProperty(ListFile::InputDirectory.name, input_dir.string());

  const auto common_timestamp = std::chrono::file_clock::now();

  const auto file_one = minifi::test::utils::putFileToDir(input_dir, "file_one.txt", "When I was one, I had just begun.");
  std::filesystem::last_write_time(file_one, common_timestamp);
  const auto result_one = test_controller.trigger();
  CHECK(result_one.at(ListFile::Success).size() == 1);

  const auto file_two = minifi::test::utils::putFileToDir(input_dir, "file_two.txt", "When I was two, I was nearly new.");
  std::filesystem::last_write_time(file_two, common_timestamp);
  const auto result_two = test_controller.trigger();
  CHECK(result_two.at(ListFile::Success).size() == 1);
}
}  // namespace
