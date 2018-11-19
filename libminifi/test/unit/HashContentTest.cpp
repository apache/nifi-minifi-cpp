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

#ifdef OPENSSL_SUPPORT

#include <uuid/uuid.h>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>
#include <iostream>

#include "../TestBase.h"
#include "core/Core.h"

#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"

#include "processors/GetFile.h"
#include "processors/HashContent.h"
#include "processors/LogAttribute.h"

const char* TEST_TEXT = "Test text";
const char* TEST_FILE = "test_file.txt";

const char* MD5_ATTR = "MD5Attr";
const char* SHA1_ATTR = "SHA1Attr";
const char* SHA256_ATTR = "SHA256Attr";

const char* MD5_CHECKSUM = "4FE8A693C64F93F65C5FAF42DC49AB23";
const char* SHA1_CHECKSUM = "03840DEB949D6CF0C0A624FA7EBA87FBDBCB7783";
const char* SHA256_CHECKSUM = "66D5B2CC06203137F8A0E9714638DC1085C57A3F1FA26C8823AE5CF89AB26488";


TEST_CASE("Test Creation of HashContent", "[HashContentCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::HashContent>("processorname");
  REQUIRE(processor->getName() == "processorname");
  utils::Identifier processoruuid;
  REQUIRE(processor->getUUID(processoruuid));
}

TEST_CASE("Test usage of ExtractText", "[extracttextTest]") {
  TestController testController;
  LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  char dir[] = "/tmp/gt.XXXXXX";

  REQUIRE(testController.createTempDirectory(dir) != nullptr);

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");

  std::shared_ptr<core::Processor> md5processor = plan->addProcessor("HashContent", "HashContentMD5",
                                                                    core::Relationship("success", "description"), true);
  plan->setProperty(md5processor, org::apache::nifi::minifi::processors::HashContent::HashAttribute.getName(), MD5_ATTR);
  plan->setProperty(md5processor, org::apache::nifi::minifi::processors::HashContent::HashAlgorithm.getName(), "MD5");

  std::shared_ptr<core::Processor> shaprocessor = plan->addProcessor("HashContent", "HashContentSHA1",
                                                                    core::Relationship("success", "description"), true);
  plan->setProperty(shaprocessor, org::apache::nifi::minifi::processors::HashContent::HashAttribute.getName(), SHA1_ATTR);
  plan->setProperty(shaprocessor, org::apache::nifi::minifi::processors::HashContent::HashAlgorithm.getName(), "sha1");

  std::shared_ptr<core::Processor> sha2processor = plan->addProcessor("HashContent", "HashContentSHA256",
                                                                    core::Relationship("success", "description"), true);
  plan->setProperty(sha2processor, org::apache::nifi::minifi::processors::HashContent::HashAttribute.getName(), SHA256_ATTR);
  plan->setProperty(sha2processor, org::apache::nifi::minifi::processors::HashContent::HashAlgorithm.getName(), "sha-256");

  std::shared_ptr<core::Processor> laprocessor = plan->addProcessor("LogAttribute", "outputLogAttribute",
                                                                    core::Relationship("success", "description"), true);

  std::stringstream ss1;
  ss1 << dir << "/" << TEST_FILE;
  std::string test_file_path = ss1.str();

  std::ofstream test_file(test_file_path);
  if (test_file.is_open()) {
    test_file << TEST_TEXT << std::endl;
    test_file.close();
  }

  for (int i = 0; i < 5; ++i) {
    plan->runNextProcessor();
  }

  std::stringstream ss2;
  ss2 << "key:" << MD5_ATTR << " value:" << MD5_CHECKSUM;
  std::string log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));

  ss2.str("");
  ss2 << "key:" << SHA1_ATTR << " value:" << SHA1_CHECKSUM;
  log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));

  ss2.str("");
  ss2 << "key:" << SHA256_ATTR << " value:" << SHA256_CHECKSUM;
  log_check = ss2.str();

  REQUIRE(LogTestController::getInstance().contains(log_check));
}

#endif  // OPENSSL_SUPPORT
