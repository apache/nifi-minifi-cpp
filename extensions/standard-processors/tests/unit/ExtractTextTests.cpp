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
#include <uuid/uuid.h>
#include <list>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include <iostream>

#include "TestBase.h"
#include "core/Core.h"

#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"

#include "GetFile.h"
#include "ExtractText.h"
#include "LogAttribute.h"

const char* TEST_TEXT = "Test text";
const char* REGEX_TEST_TEXT = "Speed limit 130 | Speed limit 80";
const char* TEST_FILE = "test_file.txt";
const char* TEST_ATTR = "ExtractedText";

TEST_CASE("Test creation of ExtractText", "[extracttextCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::ExtractText>("processorname");
  REQUIRE(processor->getName() == "processorname");
  utils::Identifier processoruuid;
  REQUIRE(processor->getUUID(processoruuid));
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

  char dirtemplate[] = "/tmp/gt.XXXXXX";

  auto temp_dir = testController.createTempDirectory(dirtemplate);
  REQUIRE(!temp_dir.empty());
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), temp_dir);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");

  std::shared_ptr<core::Processor> maprocessor = plan->addProcessor("ExtractText", "testExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::Attribute.getName(), TEST_ATTR);

  std::shared_ptr<core::Processor> laprocessor = plan->addProcessor("LogAttribute", "outputLogAttribute", core::Relationship("success", "description"), true);
  plan->setProperty(laprocessor, org::apache::nifi::minifi::processors::LogAttribute::AttributesToLog.getName(), TEST_ATTR);

  std::stringstream ss1;
  ss1 << temp_dir << utils::file::FileUtils::get_separator() << TEST_FILE;
  std::string test_file_path = ss1.str();

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

  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::SizeLimit.getName(), "4");

  LogTestController::getInstance().reset();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();

  std::ofstream test_file_2(test_file_path + "2");
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

  char dirtemplate[] = "/tmp/gt.XXXXXX";

  auto dir = testController.createTempDirectory(dirtemplate);
  REQUIRE(!dir.empty());
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");

  std::shared_ptr<core::Processor> maprocessor = plan->addProcessor("ExtractText", "testExtractText", core::Relationship("success", "description"), true);
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::RegexMode.getName(), "true");
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::IgnoreCaptureGroupZero.getName(), "true");
  plan->setProperty(maprocessor, org::apache::nifi::minifi::processors::ExtractText::EnableRepeatingCaptureGroup.getName(), "true");
  plan->setProperty(maprocessor, "RegexAttr", "Speed limit ([0-9]+)", true);
  plan->setProperty(maprocessor, "InvalidRegex", "[Invalid)A(F)", true);

  std::shared_ptr<core::Processor> laprocessor = plan->addProcessor("LogAttribute", "outputLogAttribute", core::Relationship("success", "description"), true);
  plan->setProperty(laprocessor, org::apache::nifi::minifi::processors::LogAttribute::AttributesToLog.getName(), TEST_ATTR);

  std::stringstream ss;
  ss << dir << utils::file::FileUtils::get_separator() << TEST_FILE;
  std::string test_file_path = ss.str();

  std::ofstream test_file(test_file_path);
  if (test_file.is_open()) {
    test_file << REGEX_TEST_TEXT;
    test_file.close();
  }

  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // ExtractText
  plan->runNextProcessor();  // LogAttribute

  std::list<std::string> suffixes = { "", ".0", ".1" };

  for (const auto& suffix : suffixes) {
    ss.str("");
    ss << "key:" << "RegexAttr" << suffix << " value:" << ((suffix == ".1") ? "80" : "130");
    std::string log_check = ss.str();
    REQUIRE(LogTestController::getInstance().contains(log_check));
  }

  std::string error_str = "error encountered when trying to construct regular expression from property (key: InvalidRegex)";

  REQUIRE(LogTestController::getInstance().contains(error_str));

  LogTestController::getInstance().reset();
}
