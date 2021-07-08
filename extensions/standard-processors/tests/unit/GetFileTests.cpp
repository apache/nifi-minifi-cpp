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

#include "TestBase.h"
#include "TestUtils.h"
#include "LogAttribute.h"
#include "GetFile.h"
#include "utils/file/FileUtils.h"
#include "utils/TestUtils.h"

#ifdef WIN32
#include <fileapi.h>
#endif

namespace {

class GetFileTestController {
 public:
  GetFileTestController();
  void setProperty(const core::Property& property, const std::string& value);
  void runSession();

  TestController test_controller_;
  std::shared_ptr<TestPlan> test_plan_;
  std::shared_ptr<core::Processor> get_file_processor_;
  std::string input_file_name_;
};

GetFileTestController::GetFileTestController() {
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();

  test_plan_ = test_controller_.createPlan();
  auto repo = std::make_shared<TestRepository>();

  auto temp_dir = utils::createTempDir(&test_controller_);
  REQUIRE(!temp_dir.empty());

  // Define test input file
  input_file_name_ = temp_dir + utils::file::FileUtils::get_separator() + "test.txt";
  std::string large_input_file_name = temp_dir + utils::file::FileUtils::get_separator() + "large_test_file.txt";
  std::string hidden_input_file_name = temp_dir + utils::file::FileUtils::get_separator() + ".test.txt";

  // Build MiNiFi processing graph
  get_file_processor_ = test_plan_->addProcessor("GetFile", "Get");
  test_plan_->setProperty(get_file_processor_, processors::GetFile::Directory.getName(), temp_dir);
  auto log_attr = test_plan_->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  test_plan_->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  // Write test input.
  std::ofstream in_file_stream(input_file_name_);
  in_file_stream << "The quick brown fox jumps over the lazy dog" << std::endl;
  in_file_stream.close();

  in_file_stream.open(large_input_file_name);
  in_file_stream << "The quick brown fox jumps over the lazy dog who is 2 legit to quit" << std::endl;
  in_file_stream.close();

  std::ofstream hidden_in_file_stream(hidden_input_file_name);
  hidden_in_file_stream << "But noone has ever seen it" << std::endl;
  hidden_in_file_stream.close();
#ifdef WIN32
  const auto hide_file_err = utils::file::FileUtils::hide_file(hidden_input_file_name.c_str());
  REQUIRE(!hide_file_err);
#endif
}

void GetFileTestController::setProperty(const core::Property& property, const std::string& value) {
    test_plan_->setProperty(get_file_processor_, property.getName(), value);
}

void GetFileTestController::runSession() {
  test_controller_.runSession(test_plan_);
}

}  // namespace

TEST_CASE("GetFile ignores hidden files and files larger than MaxSize", "[GetFile]") {
  GetFileTestController test_controller;
  SECTION("IgnoreHiddenFile not set, so defaults to true") {}
  SECTION("IgnoreHiddenFile set to true explicitly") { test_controller.setProperty(processors::GetFile::IgnoreHiddenFile, "true"); }
  test_controller.setProperty(processors::GetFile::MaxSize, "50 B");

  test_controller.runSession();

  REQUIRE(LogTestController::getInstance().contains("Logged 1 flow files"));  // The hidden and the too big files should be ignored
  // Check log output on windows std::endl; will produce \r\n can write manually but might as well just
  // account for the size difference here
  REQUIRE(LogTestController::getInstance().contains("key:flow.id"));
#ifdef WIN32
  REQUIRE(LogTestController::getInstance().contains("Size:45 Offset:0"));
#else
  REQUIRE(LogTestController::getInstance().contains("Size:44 Offset:0"));
#endif
}

TEST_CASE("GetFile onSchedule() throws if the required Directory property is not set", "[GetFile]") {
  TestController test_controller;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  auto plan = test_controller.createPlan();
  auto get_file = plan->addProcessor("GetFile", "Get");
  REQUIRE_THROWS_AS(plan->runNextProcessor(), minifi::Exception);
}

TEST_CASE("GetFile removes the source file if KeepSourceFile is false") {
  GetFileTestController test_controller;
  SECTION("KeepSourceFile is not set, so defaults to false") {}
  SECTION("KeepSourceFile is set to false explicitly") { test_controller.setProperty(processors::GetFile::KeepSourceFile, "false"); }

  test_controller.runSession();

  REQUIRE_FALSE(utils::file::FileUtils::exists(test_controller.input_file_name_));
}

TEST_CASE("GetFile keeps the source file if KeepSourceFile is true") {
  GetFileTestController test_controller;
  test_controller.setProperty(processors::GetFile::KeepSourceFile, "true");

  test_controller.runSession();

  REQUIRE(utils::file::FileUtils::exists(test_controller.input_file_name_));
}

TEST_CASE("GetFileHiddenPropertyCheck", "[getFileProperty]") {
  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<processors::LogAttribute>();
  auto plan = testController.createPlan();

  auto temp_path = minifi::utils::createTempDir(&testController);
  std::string in_file(temp_path + utils::file::FileUtils::get_separator() + "testfifo");
  std::string hidden_in_file(temp_path + utils::file::FileUtils::get_separator() + ".testfifo");

  auto get_file = plan->addProcessor("GetFile", "Get");
  plan->setProperty(get_file, processors::GetFile::IgnoreHiddenFile.getName(), "false");

  plan->setProperty(get_file, processors::GetFile::Directory.getName(), temp_path);
  auto log_attr = plan->addProcessor("LogAttribute", "Log", core::Relationship("success", "description"), true);
  plan->setProperty(log_attr, processors::LogAttribute::FlowFilesToLog.getName(), "0");

  std::ofstream in_file_stream(in_file);
  in_file_stream << "This file is not hidden" << std::endl;
  in_file_stream.close();

  std::ofstream hidden_in_file_stream(hidden_in_file);
  hidden_in_file_stream << "This file is hidden" << std::endl;
  hidden_in_file_stream.close();

  plan->runNextProcessor();
  plan->runNextProcessor();

  REQUIRE(LogTestController::getInstance().contains("Logged 2 flow files"));
}
