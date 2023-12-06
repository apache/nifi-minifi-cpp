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
#include "SingleProcessorTestController.h"
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

  plan->setProperty(putfile, minifi::processors::PutFile::Directory, dir.string());

  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::FileSize, "10");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::BatchSize, "2");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "true");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::DataFormat, "Text");

  // This property will be ignored if unique flow files are used
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::CustomText, "Current time: ${now()}");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
    std::ifstream is(path / filename, std::ifstream::binary);
    file_contents.emplace_back(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
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

  plan->setProperty(putfile, minifi::processors::PutFile::Directory, dir.string());

  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::FileSize, "10");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::BatchSize, "2");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "false");

  // This property will be ignored if binary files are used
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::CustomText, "Current time: ${now()}");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::vector<char>> fileContents;

  auto lambda = [&fileContents](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
    std::ifstream is(path / filename, std::ifstream::binary);

    is.seekg(0, std::ifstream::end);
    const auto length = is.tellg();
    is.seekg(0, std::ifstream::beg);

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

  const auto dir = testController.createTempDirectory();

  const std::shared_ptr<TestPlan> plan = testController.createPlan();

  const std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  const std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, minifi::processors::PutFile::Directory, dir.string());

  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::FileSize, "0");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put

  size_t counter = 0;

  auto lambda = [&counter](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
    std::ifstream is(path / filename, std::ifstream::binary);

    is.seekg(0, std::ifstream::end);
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

  const auto dir = test_controller.createTempDirectory();

  const std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  const std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  const std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, minifi::processors::PutFile::Directory, dir.string());

  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::CustomText, "${UUID()}");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "false");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::DataFormat, "Text");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
    std::ifstream is(path / filename, std::ifstream::binary);
    file_contents.emplace_back(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == 36);
}

TEST_CASE("GenerateFlowFileCustomTextEmptyTest", "[generateflowfiletest]") {
  TestController test_controller;
  LogTestController::getInstance().setTrace<TestPlan>();

  const auto dir = test_controller.createTempDirectory();

  const std::shared_ptr<TestPlan> plan = test_controller.createPlan();

  const std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  const std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, minifi::processors::PutFile::Directory, dir.string());

  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::FileSize, "10");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::CustomText, "");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::UniqueFlowFiles, "false");
  plan->setProperty(genfile, minifi::processors::GenerateFlowFile::DataFormat, "Text");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::string> file_contents;

  auto lambda = [&file_contents](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
    std::ifstream is(path / filename, std::ifstream::binary);
    file_contents.emplace_back(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
    return true;
  };

  utils::file::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(file_contents.size() == 1);
  REQUIRE(file_contents[0].size() == 10);
}

TEST_CASE("GenerateFlowFile should reevaluate CustomText once per batch", "[generateflowfiletest]") {
  using minifi::processors::GenerateFlowFile;
  std::shared_ptr<GenerateFlowFile> generate_flow_file = std::make_shared<GenerateFlowFile>("GenerateFlowFile");
  minifi::test::SingleProcessorTestController test_controller{generate_flow_file};
  test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::DataFormat, "Text");
  test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::UniqueFlowFiles, "false");
  test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::CustomText, "${now()}");
  test_controller.plan->setProperty(generate_flow_file, GenerateFlowFile::BatchSize, "2");

  auto first_batch = test_controller.trigger();
  REQUIRE(first_batch.at(GenerateFlowFile::Success).size() == 2);
  auto first_batch_0 = test_controller.plan->getContent(first_batch.at(GenerateFlowFile::Success)[0]);
  auto first_batch_1 = test_controller.plan->getContent(first_batch.at(GenerateFlowFile::Success)[1]);
  CHECK(first_batch_0 == first_batch_1);

  std::this_thread::sleep_for(2ms);
  auto second_batch = test_controller.trigger();
  REQUIRE(second_batch.at(GenerateFlowFile::Success).size() == 2);
  auto second_batch_0 = test_controller.plan->getContent(second_batch.at(GenerateFlowFile::Success)[0]);
  auto second_batch_1 = test_controller.plan->getContent(second_batch.at(GenerateFlowFile::Success)[1]);
  CHECK(second_batch_0 == second_batch_1);

  CHECK(first_batch_0 != second_batch_0);
}
