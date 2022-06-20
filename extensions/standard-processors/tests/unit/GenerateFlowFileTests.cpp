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
#include <vector>
#include <set>
#include <fstream>

#include "TestBase.h"
#include "Catch.h"
#include "utils/file/FileUtils.h"
#include "GenerateFlowFile.h"
#include "PutFile.h"

TEST_CASE("GenerateFlowFileTest", "[generateflowfiletest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setWarn<minifi::processors::GenerateFlowFile>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "10");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::BatchSize.getName(), "2");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::UniqueFlowFiles.getName(), "true");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::DataFormat.getName(), "Text");

  // This property will be ignored if unique flow files are used
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::CustomText.getName(), "Current time: ${now()}");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::get_separator() + filename, std::ifstream::binary);
    file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(file_contents.size() == 2);
  REQUIRE(file_contents[0].size() == 10);
  REQUIRE(file_contents[1].size() == 10);
  REQUIRE(file_contents[0] != file_contents[1]);
  REQUIRE(LogTestController::getInstance().contains("Custom Text property is set, but not used!"));
}

TEST_CASE("GenerateFlowFileWithNonUniqueBinaryData", "[generateflowfiletest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setWarn<minifi::processors::GenerateFlowFile>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "10");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::BatchSize.getName(), "2");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::UniqueFlowFiles.getName(), "false");

  // This property will be ignored if binary files are used
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::CustomText.getName(), "Current time: ${now()}");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::vector<char>> fileContents;

  auto lambda = [&fileContents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::get_separator() + filename, std::ifstream::binary);

    is.seekg(0, is.end);
    auto length = gsl::narrow<size_t>(is.tellg());
    is.seekg(0, is.beg);

    std::vector<char> content(length);

    is.read(content.data(), length);

    fileContents.push_back(std::move(content));

    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(fileContents.size() == 2);
  REQUIRE(fileContents[0].size() == 10);
  REQUIRE(fileContents[1].size() == 10);
  REQUIRE(fileContents[0] == fileContents[1]);
  REQUIRE(LogTestController::getInstance().contains("Custom Text property is set, but not used!"));
}

TEST_CASE("GenerateFlowFileTestEmpty", "[generateemptyfiletest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();

  auto dir = testController.createTempDirectory();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "0");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put

  size_t counter = 0;

  auto lambda = [&counter](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::get_separator() + filename, std::ifstream::binary);

    is.seekg(0, is.end);
    REQUIRE(is.tellg() == 0);

    counter++;

    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(counter == 1);
}

TEST_CASE("GenerateFlowFileCustomTextTest", "[generateflowfiletest]") {
  TestController test_controller;
  LogTestController::getInstance().setTrace<TestPlan>();

  auto dir = test_controller.createTempDirectory();

  std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::CustomText.getName(), "${UUID()}");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::UniqueFlowFiles.getName(), "false");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::DataFormat.getName(), "Text");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::get_separator() + filename, std::ifstream::binary);
    file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == 36);
}

TEST_CASE("GenerateFlowFileCustomTextEmptyTest", "[generateflowfiletest]") {
  TestController test_controller;
  LogTestController::getInstance().setTrace<TestPlan>();

  auto dir = test_controller.createTempDirectory();

  std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "10");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::CustomText.getName(), "");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::UniqueFlowFiles.getName(), "false");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::DataFormat.getName(), "Text");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::get_separator() + filename, std::ifstream::binary);
    file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == 10);
}
