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
#include <list>
#include <fstream>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include <iostream>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "unit/ProvenanceTestHelper.h"
#include "repository/VolatileContentRepository.h"
#include "unit/TestUtils.h"

#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"

#include "GetFile.h"
#include "ExtractText.h"
#include "LogAttribute.h"

const char* TEST_TEXT = "Test text";
const char* REGEX_TEST_TEXT = "Speed limit 130 | Speed limit 80";
const char* TEST_FILE = "test_file.txt";
const char* TEST_ATTR = "ExtractedText";

TEST_CASE("Test creation of ExtractText", "[extracttextCreate]") {
  TestController testController;
  auto processor = std::make_unique<org::apache::nifi::minifi::processors::ExtractText>("processorname");
  REQUIRE(processor->getName() == "processorname");
  utils::Identifier processoruuid = processor->getUUID();
  REQUIRE(processoruuid);
}

TEST_CASE("Test usage of ExtractText", "[extracttextTest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::ExtractText>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::FlowFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  auto temp_dir = testController.createTempDirectory();
  REQUIRE(!temp_dir.empty());
  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, temp_dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile, "true");

  auto maprocessor = plan->addProcessor("ExtractText", "testExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::Attribute, TEST_ATTR);

  plan->addProcessor("LogAttribute", "outputLogAttribute", core::Relationship("success", "description"), true);

  auto test_file_path = temp_dir / TEST_FILE;

  std::ofstream test_file(test_file_path);
  if (test_file.is_open()) {
    test_file << TEST_TEXT;
    test_file.close();
  }

  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // ExtractText
  plan->runNextProcessor();  // LogAttribute

  std::stringstream ss2;
  ss2 << "key:" << TEST_ATTR << " value:" << TEST_TEXT;
  std::string log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));

  plan->reset();

  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::SizeLimit, "4");

  LogTestController::getInstance().reset();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();

  std::ofstream test_file_2(test_file_path.string() + "2");
  if (test_file_2.is_open()) {
    test_file_2 << TEST_TEXT << std::endl;
    test_file_2.close();
  }

  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // ExtractText
  plan->runNextProcessor();  // LogAttribute

  REQUIRE(LogTestController::getInstance().contains(log_check, std::chrono::seconds(0)) == false);

  ss2.str("");
  ss2 << "key:" << TEST_ATTR << " value:" << "Test";
  log_check = ss2.str();
  REQUIRE(LogTestController::getInstance().contains(log_check));

  LogTestController::getInstance().reset();
}

TEST_CASE("Test usage of ExtractText in regex mode", "[extracttextRegexTest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::ExtractText>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  auto dir = testController.createTempDirectory();
  REQUIRE(!dir.empty());
  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile, "true");

  auto maprocessor = plan->addProcessor("ExtractText", "testExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::RegexMode, "true");
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::EnableRepeatingCaptureGroup, "true");
  plan->setDynamicProperty(maprocessor, "RegexAttr", "Speed limit ([0-9]+)");
  plan->setDynamicProperty(maprocessor, "InvalidRegex", "[Invalid)A(F)");

  plan->addProcessor("LogAttribute", "outputLogAttribute", core::Relationship("success", "description"), true);

  auto test_file_path = dir / TEST_FILE;

  std::ofstream test_file(test_file_path);
  if (test_file.is_open()) {
    test_file << REGEX_TEST_TEXT;
    test_file.close();
  }

  std::list<std::string> expected_logs;

  SECTION("Do not include capture group 0") {
    plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::IncludeCaptureGroupZero, "false");

    testController.runSession(plan);

    expected_logs = {
      "key:RegexAttr value:130",
      "key:RegexAttr.0 value:130",
      "key:RegexAttr.1 value:80"
    };
  }

  SECTION("Include capture group 0") {
    testController.runSession(plan);

    expected_logs = {
      "key:RegexAttr value:Speed limit 130",
      "key:RegexAttr.0 value:Speed limit 130",
      "key:RegexAttr.1 value:130",
      "key:RegexAttr.2 value:Speed limit 80",
      "key:RegexAttr.3 value:80"
    };
  }

  for (const auto& log : expected_logs) {
    REQUIRE(LogTestController::getInstance().contains(log));
  }

  std::string error_str = "error encountered when trying to construct regular expression from property (key: InvalidRegex)";

  REQUIRE(LogTestController::getInstance().contains(error_str));

  LogTestController::getInstance().reset();
}

TEST_CASE("Test usage of ExtractText in regex mode with large regex matches", "[extracttextRegexTest]") {
  TestController test_controller;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::ExtractText>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = test_controller.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  auto dir = test_controller.createTempDirectory();
  REQUIRE(!dir.empty());
  auto getfile = plan->addProcessor("GetFile", "GetFile");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile, "true");

  auto extract_text_processor = plan->addProcessor("ExtractText", "ExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(extract_text_processor, org::apache::nifi::minifi::processors::ExtractText::RegexMode, "true");
  plan->setProperty(extract_text_processor, org::apache::nifi::minifi::processors::ExtractText::IncludeCaptureGroupZero, "false");
  plan->setDynamicProperty(extract_text_processor, "RegexAttr", "Speed limit (.*)");

  plan->addProcessor("LogAttribute", "outputLogAttribute", core::Relationship("success", "description"), true);

  std::string additional_long_string(100'000, '.');
  minifi::test::utils::putFileToDir(dir, TEST_FILE, "Speed limit 80" + additional_long_string);

  test_controller.runSession(plan);

  REQUIRE(LogTestController::getInstance().contains("key:RegexAttr.0 value:80"));
  LogTestController::getInstance().reset();
}
