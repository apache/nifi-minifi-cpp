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
#include "utils/file/FileUtils.h"
#include "GenerateFlowFile.h"
#include "PutFile.h"

TEST_CASE("GenerateFlowFileTest", "[generateflowfiletest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "10");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::BatchSize.getName(), "2");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::UniqueFlowFiles.getName(), "true");
  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::DataFormat.getName(), "Text");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put
  plan->runCurrentProcessor();  // Put

  std::vector<std::vector<char>> fileContents;

  auto lambda = [&fileContents](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);

    is.seekg(0, is.end);
    size_t length = is.tellg();
    is.seekg(0, is.beg);

    std::vector<char> content(length);

    is.read(&content[0], length);

    fileContents.push_back(std::move(content));

    return true;
  };

  utils::file::FileUtils::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(fileContents.size() == 2);
  REQUIRE(fileContents[0].size() == 10);
  REQUIRE(fileContents[1].size() == 10);
  REQUIRE(fileContents[0] != fileContents[1]);
}

TEST_CASE("GenerateFlowFileTestEmpty", "[generateemptyfiletest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<TestPlan>();

  char format[] = "/tmp/gt.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> genfile = plan->addProcessor("GenerateFlowFile", "genfile");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir);

  plan->setProperty(genfile, org::apache::nifi::minifi::processors::GenerateFlowFile::FileSize.getName(), "0");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // Put

  size_t counter = 0;

  auto lambda = [&counter](const std::string& path, const std::string& filename) -> bool {
    std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);

    is.seekg(0, is.end);
    REQUIRE(is.tellg() == 0);

    counter++;

    return true;
  };

  utils::file::FileUtils::list_dir(dir, lambda, plan->getLogger(), false);

  REQUIRE(counter == 1);
}
