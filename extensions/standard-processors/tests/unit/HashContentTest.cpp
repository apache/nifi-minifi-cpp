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
#include <array>
#include <memory>
#include <utility>
#include <string>
#include <iostream>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/SingleProcessorTestController.h"
#include "unit/TestUtils.h"
#include "unit/ProvenanceTestHelper.h"

#include "core/Processor.h"
#include "core/ProcessSession.h"

#include "GetFile.h"
#include "HashContent.h"
#include "LogAttribute.h"

const char* TEST_TEXT = "Test text";
const char* TEST_FILE = "test_file.txt";

const char* MD5_ATTR = "MD5Attr";
const char* SHA1_ATTR = "SHA1Attr";
const char* SHA256_ATTR = "SHA256Attr";

const char* MD5_CHECKSUM = "4FE8A693C64F93F65C5FAF42DC49AB23";
const char* SHA1_CHECKSUM = "03840DEB949D6CF0C0A624FA7EBA87FBDBCB7783";
const char* SHA256_CHECKSUM = "66D5B2CC06203137F8A0E9714638DC1085C57A3F1FA26C8823AE5CF89AB26488";

namespace org::apache::nifi::minifi::processors::test {

TEST_CASE("Test Creation of HashContent", "[HashContentCreate]") {
  TestController testController;
  auto processor = std::make_shared<org::apache::nifi::minifi::processors::HashContent>("processorname");
  REQUIRE(processor->getName() == "processorname");
  REQUIRE(processor->getUUID());
}

TEST_CASE("Test usage of ExtractText", "[extracttextTest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::HashContent>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  auto tempdir = testController.createTempDirectory();
  REQUIRE(!tempdir.empty());

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, tempdir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile, "true");

  auto md5processor = plan->addProcessor("HashContent", "HashContentMD5",
      core::Relationship("success", "description"), true);
  plan->setProperty(md5processor, org::apache::nifi::minifi::processors::HashContent::HashAttribute, MD5_ATTR);
  plan->setProperty(md5processor, org::apache::nifi::minifi::processors::HashContent::HashAlgorithm, "MD5");

  auto shaprocessor = plan->addProcessor("HashContent", "HashContentSHA1",
      core::Relationship("success", "description"), true);
  plan->setProperty(shaprocessor, org::apache::nifi::minifi::processors::HashContent::HashAttribute, SHA1_ATTR);
  plan->setProperty(shaprocessor, org::apache::nifi::minifi::processors::HashContent::HashAlgorithm, "sha1");

  auto sha2processor = plan->addProcessor("HashContent", "HashContentSHA256",
      core::Relationship("success", "description"), true);
  plan->setProperty(sha2processor, org::apache::nifi::minifi::processors::HashContent::HashAttribute, SHA256_ATTR);
  plan->setProperty(sha2processor, org::apache::nifi::minifi::processors::HashContent::HashAlgorithm, "sha-256");

  plan->addProcessor("LogAttribute", "outputLogAttribute", core::Relationship("success", "description"), true);

  auto test_file_path = tempdir / TEST_FILE;

  std::ofstream test_file(test_file_path, std::ios::binary);

  if (test_file.is_open()) {
    test_file << TEST_TEXT;
    // the standard allows << std::endl; to replace, in line, with \r\n on windows
    char newline = '\n';
    test_file.write(&newline, 1);
    test_file.close();
  }

  for (int i = 0; i < 5; ++i) {
    plan->runNextProcessor();
  }

  std::stringstream ss2;
  ss2 << "key:" << MD5_ATTR << " value:" << MD5_CHECKSUM << "\n";
  std::string log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));

  ss2.str("");
  ss2 << "key:" << SHA1_ATTR << " value:" << SHA1_CHECKSUM << "\n";
  log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));

  ss2.str("");
  ss2 << "key:" << SHA256_ATTR << " value:" << SHA256_CHECKSUM << "\n";
  log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));
}

TEST_CASE("TestingFailOnEmptyProperty", "[HashContentPropertiesCheck]") {
  using minifi::processors::HashContent;
  TestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::HashContent>();
  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto tempdir = testController.createTempDirectory();
  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, tempdir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile, "true");

  auto md5processor = plan->addProcessor("HashContent", "HashContentMD5",
                                                                     core::Relationship("success", "description"), true);
  plan->setProperty(md5processor, HashContent::HashAttribute, MD5_ATTR);
  plan->setProperty(md5processor, HashContent::HashAlgorithm, "MD5");

  md5processor->setAutoTerminatedRelationships(std::array<core::Relationship, 2>{HashContent::Success, HashContent::Failure});

  auto test_file_path = tempdir / TEST_FILE;
  std::ofstream test_file(test_file_path, std::ios::binary);

  SECTION("with an empty file and fail on empty property set to false") {
    plan->setProperty(md5processor, HashContent::FailOnEmpty, "false");

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE(LogTestController::getInstance().contains("attempting read"));
  }
  SECTION("with an empty file and fail on empty property set to true") {
    plan->setProperty(md5processor, HashContent::FailOnEmpty, "true");

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE(LogTestController::getInstance().contains("Failure as flow file is empty"));
  }
}

TEST_CASE("Invalid hash algorithm throws in onSchedule", "[HashContent]") {
  minifi::test::SingleProcessorTestController controller{std::make_unique<HashContent>("HashContent")};
  auto hash_content = controller.getProcessor();
  hash_content->setProperty(HashContent::HashAlgorithm.name, "My-Algo");
  REQUIRE_THROWS_WITH(controller.plan->scheduleProcessor(hash_content), "Process Schedule Operation: MYALGO is not supported, supported algorithms are: MD5, SHA1, SHA256");
}

}  // namespace org::apache::nifi::minifi::processors::test
