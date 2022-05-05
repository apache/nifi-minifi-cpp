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

#include "TestBase.h"
#include "Catch.h"
#include "core/Property.h"
#include "core/Processor.h"
#include "processors/LogAttribute.h"
#include "processors/ListFile.h"
#include "utils/TestUtils.h"
#include "utils/IntegrationTestUtils.h"
#include "utils/file/PathUtils.h"

using namespace std::literals::chrono_literals;

namespace {

using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;

class ListFileTestFixture {
 public:
  static const std::string FORMAT_STRING;
  ListFileTestFixture();

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  const std::string input_dir_;
  std::shared_ptr<core::Processor> list_file_processor_;
  std::string hidden_file_path_;
  std::string empty_file_abs_path_;
  std::string standard_file_abs_path_;
  std::string first_sub_file_abs_path_;
  std::string second_sub_file_abs_path_;
};

const std::string ListFileTestFixture::FORMAT_STRING = "%Y-%m-%dT%H:%M:%SZ";

ListFileTestFixture::ListFileTestFixture()
  : plan_(test_controller_.createPlan()),
    input_dir_(test_controller_.createTempDirectory()) {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<minifi::processors::ListFile>();
  LogTestController::getInstance().setTrace<minifi::processors::LogAttribute>();

  REQUIRE(!input_dir_.empty());

  list_file_processor_ = plan_->addProcessor("ListFile", "ListFile");
  plan_->setProperty(list_file_processor_, "Input Directory", input_dir_);
  auto log_attribute = plan_->addProcessor("LogAttribute", "logAttribute", core::Relationship("success", "description"), true);
  plan_->setProperty(log_attribute, "FlowFiles To Log", "0");

  hidden_file_path_ = utils::putFileToDir(input_dir_, ".hidden_file.txt", "hidden");
  standard_file_abs_path_ = utils::putFileToDir(input_dir_, "standard_file.log", "test");
  empty_file_abs_path_ = utils::putFileToDir(input_dir_, "empty_file.txt", "");
  utils::file::FileUtils::create_dir(input_dir_ + utils::file::FileUtils::get_separator() + "first_subdir");
  first_sub_file_abs_path_ = utils::putFileToDir(input_dir_ + utils::file::FileUtils::get_separator() + "first_subdir", "sub_file_one.txt", "the");
  utils::file::FileUtils::create_dir(input_dir_ + utils::file::FileUtils::get_separator() + "second_subdir");
  second_sub_file_abs_path_ = utils::putFileToDir(input_dir_ + utils::file::FileUtils::get_separator() + "second_subdir", "sub_file_two.txt", "some_other_content");

  auto last_write_time = *utils::file::FileUtils::last_write_time(standard_file_abs_path_);
  utils::file::FileUtils::set_last_write_time(empty_file_abs_path_, last_write_time - 1h);
  utils::file::FileUtils::set_last_write_time(first_sub_file_abs_path_, last_write_time - 2h);
  utils::file::FileUtils::set_last_write_time(second_sub_file_abs_path_, last_write_time - 3h);
#ifndef WIN32
  REQUIRE(0 == utils::file::FileUtils::set_permissions(input_dir_ + utils::file::FileUtils::get_separator() + "empty_file.txt", 0755));
  REQUIRE(0 == utils::file::FileUtils::set_permissions(input_dir_ + utils::file::FileUtils::get_separator() + "standard_file.log", 0644));
#endif

#ifdef WIN32
  const auto hide_file_error = utils::file::FileUtils::hide_file(hidden_file_path_.c_str());
  REQUIRE(!hide_file_error);
#endif
}

TEST_CASE_METHOD(ListFileTestFixture, "Input Directory is empty", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Input Directory", "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files only once with default parameters", "[testListFile]") {
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  std::string file_path;
  std::string file_name;
  utils::file::getFileNameAndPath(empty_file_abs_path_, file_path, file_name);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + file_path + utils::file::FileUtils::get_separator() + "\n"));
  utils::file::getFileNameAndPath(standard_file_abs_path_, file_path, file_name);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + file_path + utils::file::FileUtils::get_separator() + "\n"));
  utils::file::getFileNameAndPath(first_sub_file_abs_path_, file_path, file_name);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + file_path + utils::file::FileUtils::get_separator() + "\n"));
  utils::file::getFileNameAndPath(second_sub_file_abs_path_, file_path, file_name);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:absolute.path value:" + file_path + utils::file::FileUtils::get_separator() + "\n"));
  REQUIRE(LogTestController::getInstance().countOccurrences(std::string("key:path value:.") + utils::file::FileUtils::get_separator() + "\n") == 2);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, std::string("key:path value:first_subdir") + utils::file::FileUtils::get_separator()));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, std::string("key:path value:second_subdir") + utils::file::FileUtils::get_separator()));
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
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + *utils::file::FileUtils::get_last_modified_time_formatted_string(empty_file_abs_path_, FORMAT_STRING)));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + *utils::file::FileUtils::get_last_modified_time_formatted_string(standard_file_abs_path_, FORMAT_STRING)));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + *utils::file::FileUtils::get_last_modified_time_formatted_string(first_sub_file_abs_path_, FORMAT_STRING)));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:file.lastModifiedTime value:" + *utils::file::FileUtils::get_last_modified_time_formatted_string(second_sub_file_abs_path_, FORMAT_STRING)));
  plan_->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
  test_controller_.runSession(plan_, true);
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:file.size", 0s, 0ms));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test turning off recursive file listing", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Recurse Subdirectories", "false");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_one.txt", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_two.txt", 0s, 0ms));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files matching the File Filter pattern", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "File Filter", "stand\\w+\\.log");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:empty_file.txt", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_one.txt", 0s, 0ms));
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:filename value:sub_file_two.txt", 0s, 0ms));
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files matching the Path Filter pattern", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Path Filter", "first.*");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_two.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the minimum file age", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Minimum File Age", "90 min");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:empty_file.txt") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:standard_file.log") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the maximum file age", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Maximum File Age", "90 min");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_one.txt") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_two.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the minimum file size", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Minimum File Size", "4 B");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:empty_file.txt") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_one.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing files with restriction on the maximum file size", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Maximum File Size", "4 B");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:sub_file_two.txt") == 0);
}

TEST_CASE_METHOD(ListFileTestFixture, "Test listing hidden files", "[testListFile]") {
  plan_->setProperty(list_file_processor_, "Ignore Hidden Files", "false");
  test_controller_.runSession(plan_);
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:standard_file.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:empty_file.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_one.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:sub_file_two.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(3s, "key:filename value:.hidden_file.txt"));
}

}  // namespace
