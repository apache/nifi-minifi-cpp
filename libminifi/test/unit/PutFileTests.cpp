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
#include <sys/stat.h>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>


#include "utils/file/FileUtils.h"
#include "../TestBase.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "../unit/ProvenanceTestHelper.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"

TEST_CASE("Test Creation of PutFile", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::PutFile>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("PutFileTest", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile::ReadCallback>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  char format2[] = "/tmp/ft.XXXXXX";
  char *putfiledir = testController.createTempDirectory(format2);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  std::stringstream movedFile;
  movedFile << putfiledir << "/" << "tstFile.ext";
  REQUIRE(true == std::ifstream(movedFile.str()).good());

  file.open(movedFile.str(), std::ios::in);
  std::string contents((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
  REQUIRE("tempFile" == contents);
  file.close();
  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileTestFileExists", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("failure", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  char format2[] = "/tmp/ft.XXXXXX";
  char *putfiledir = testController.createTempDirectory(format2);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
//
  std::stringstream movedFile;
  movedFile << putfiledir << "/" << "tstFile.ext";
  file.open(movedFile.str(), std::ios::out);
  file << "tempFile";
  file.close();

  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  REQUIRE(true == std::ifstream(movedFile.str()).good());

  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileTestFileExistsIgnore", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  char format2[] = "/tmp/ft.XXXXXX";
  char *putfiledir = testController.createTempDirectory(format2);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::ConflictResolution.getName(), "ignore");

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
//
  std::stringstream movedFile;
  movedFile << putfiledir << "/" << "tstFile.ext";
  file.open(movedFile.str(), std::ios::out);
  file << "tempFile";
  file.close();
  auto filemodtime = fileutils::FileUtils::last_write_time(movedFile.str());

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  REQUIRE(true == std::ifstream(movedFile.str()).good());
  REQUIRE(filemodtime == fileutils::FileUtils::last_write_time(movedFile.str()));
  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileTestFileExistsReplace", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  char format[] = "/tmp/gt.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  char format2[] = "/tmp/ft.XXXXXX";
  char *putfiledir = testController.createTempDirectory(format2);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::ConflictResolution.getName(), "replace");

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
//
  std::stringstream movedFile;
  movedFile << putfiledir << "/" << "tstFile.ext";
  file.open(movedFile.str(), std::ios::out);
  file << "tempFile";
  file.close();
  auto filemodtime = fileutils::FileUtils::last_write_time(movedFile.str());

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  unlink(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  REQUIRE(true == std::ifstream(movedFile.str()).good());
  REQUIRE(filemodtime != fileutils::FileUtils::last_write_time(movedFile.str()));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test generation of temporary write path", "[putfileTmpWritePath]") {
  auto processor = std::make_shared<org::apache::nifi::minifi::processors::PutFile>("processorname");
  REQUIRE(processor->tmpWritePath("a/b/c", "").substr(1, strlen("a/b/.c")) == "a/b/.c");
}

