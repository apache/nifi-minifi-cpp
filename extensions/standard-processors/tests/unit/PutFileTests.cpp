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
#include <cstdio>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <fstream>

#include "utils/file/FileUtils.h"
#include "TestBase.h"
#include "Catch.h"
#include "TestUtils.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Core.h"
#include "core/FlowFile.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessorNode.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "Exception.h"

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

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << utils::file::get_separator() << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  std::remove(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  std::stringstream movedFile;
  movedFile << putfiledir << utils::file::get_separator() << "tstFile.ext";
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

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.size() == 0);

  std::fstream file;
  std::stringstream ss;
  ss << dir << utils::file::get_separator() << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
//
  std::stringstream movedFile;
  movedFile << putfiledir << utils::file::get_separator() << "tstFile.ext";
  file.open(movedFile.str(), std::ios::out);
  file << "tempFile";
  file.close();

  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  std::remove(ss.str().c_str());

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

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
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
  ss << dir << utils::file::get_separator() << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
//
  std::stringstream movedFile;
  movedFile << putfiledir << utils::file::get_separator() << "tstFile.ext";
  file.open(movedFile.str(), std::ios::out);
  file << "tempFile";
  file.close();
  auto filemodtime = utils::file::last_write_time(movedFile.str());

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  std::remove(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + dir));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  REQUIRE(true == std::ifstream(movedFile.str()).good());
  REQUIRE(filemodtime == utils::file::last_write_time(movedFile.str()));
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

  plan->addProcessor("LogAttribute", "logattribute", { core::Relationship("success", "d"), core::Relationship("failure", "d") }, true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
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
  ss << dir << utils::file::get_separator() << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();
//
  std::stringstream movedFile;
  movedFile << putfiledir << utils::file::get_separator() << "tstFile.ext";
  file.open(movedFile.str(), std::ios::out);
  file << "tempFile";
  file.close();
  auto filemodtime = utils::file::last_write_time(movedFile.str());

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  records = plan->getProvenanceRecords();
  record = plan->getCurrentFlowFile();
  testController.runSession(plan, false);

  std::remove(ss.str().c_str());

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + ss.str()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + std::string(dir)));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(ss.str()).good());
  REQUIRE(true == std::ifstream(movedFile.str()).good());
#ifndef WIN32
  REQUIRE(filemodtime != utils::file::last_write_time(movedFile.str()));
#endif
  LogTestController::getInstance().reset();
}

TEST_CASE("Test generation of temporary write path", "[putfileTmpWritePath]") {
  auto processor = std::make_shared<org::apache::nifi::minifi::processors::PutFile>("processorname");
  std::stringstream prefix;
  prefix << "a" << utils::file::get_separator() << "b" << utils::file::get_separator();
  std::string path = prefix.str() + "c";
  std::string expected_path = prefix.str() + ".c";
  REQUIRE(processor->tmpWritePath(path, "").substr(1, expected_path.length()) == expected_path);
}

TEST_CASE("PutFileMaxFileCountTest", "[getfileputpfilemaxcount]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", { core::Relationship("success", "d"), core::Relationship("failure", "d") }, true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::BatchSize.getName(), "1");
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::MaxDestFiles.getName(), "1");



  for (int i = 0; i < 2; ++i) {
    std::stringstream ss;
    ss << dir << utils::file::get_separator() << "tstFile" << i << ".ext";
    std::fstream file;
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();
  }

  plan->reset();

  testController.runSession(plan);

  plan->reset();

  testController.runSession(plan);


  REQUIRE(LogTestController::getInstance().contains("key:absolute.path value:" + std::string(dir) + utils::file::get_separator() + "tstFile0.ext"));
  REQUIRE(LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(LogTestController::getInstance().contains("key:path value:" + std::string(dir)));

  // Only 1 of the 2 files should make it to the target dir
  // Non-determistic, so let's just count them
  int files_in_dir = 0;

  for (int i = 0; i < 2; ++i) {
    std::stringstream ss;
    ss << putfiledir << utils::file::get_separator() << "tstFile" << i << ".ext";
    std::ifstream file(ss.str());
    if (file.is_open() && file.good()) {
      files_in_dir++;
      file.close();
    }
  }

  REQUIRE(files_in_dir == 1);

  REQUIRE(LogTestController::getInstance().contains("which exceeds the configured max number of files"));

  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileEmptyTest", "[EmptyFilePutTest]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);

  std::ofstream of(std::string(dir) + utils::file::get_separator() + "tstFile.ext");
  of.close();

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Put

  std::ifstream is(std::string(putfiledir) + utils::file::get_separator() + "tstFile.ext", std::ifstream::binary);

  REQUIRE(is.is_open());
  is.seekg(0, is.end);
  REQUIRE(is.tellg() == 0);
}

#ifndef WIN32
TEST_CASE("TestPutFilePermissions", "[PutFilePermissions]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");

  std::shared_ptr<core::Processor> putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory() + utils::file::get_separator() + "test_dir";

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Permissions.getName(), "644");
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::DirectoryPermissions.getName(), "0777");

  std::fstream file;
  file.open(std::string(dir) + utils::file::FileUtils::get_separator() + "tstFile.ext", std::ios::out);
  file << "tempFile";
  file.close();

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Put

  auto path = std::string(putfiledir) + utils::file::FileUtils::get_separator() + "tstFile.ext";
  uint32_t perms = 0;
  REQUIRE(utils::file::FileUtils::get_permissions(path, perms));
  REQUIRE(perms == 0644);
  REQUIRE(utils::file::FileUtils::get_permissions(putfiledir, perms));
  REQUIRE(perms == 0777);
}

TEST_CASE("PutFileCreateDirectoryTest", "[PutFileProperties]") {
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

  // Define Directory
  auto dir = testController.createTempDirectory();
  // Defining a sub directory
  auto putfiledir = testController.createTempDirectory() + utils::file::FileUtils::get_separator() + "test_dir";

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), putfiledir);
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir);

  SECTION("with an empty file and create directory property set to true") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs.getName(), "true");

    std::ofstream of(std::string(dir) + utils::file::FileUtils::get_separator() + "tstFile.ext");
    of.close();
    auto path = std::string(putfiledir) + utils::file::FileUtils::get_separator() + "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE(utils::file::exists(putfiledir));
    REQUIRE(utils::file::exists(path));
  }

  SECTION("with an empty file and create directory property set to false") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs.getName(), "false");
    putfile->setAutoTerminatedRelationships(std::array{core::Relationship("failure", "description")});

    std::ofstream of(std::string(dir) + utils::file::FileUtils::get_separator() + "tstFile.ext");
    of.close();
    auto path = std::string(putfiledir) + utils::file::FileUtils::get_separator() + "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE_FALSE(utils::file::exists(putfiledir));
    REQUIRE_FALSE(utils::file::exists(path));
    std::string check = "Failed to create empty file: " + path;
    REQUIRE(LogTestController::getInstance().contains(check));
  }

  SECTION("with a non-empty file and create directory property set to true") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs.getName(), "true");

    std::ofstream of(std::string(dir) + utils::file::FileUtils::get_separator() + "tstFile.ext");
    of << "tempFile";
    of.close();
    auto path = std::string(putfiledir) + utils::file::FileUtils::get_separator() + "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE(utils::file::exists(putfiledir));
    REQUIRE(utils::file::exists(path));
  }

  SECTION("with a non-empty file and create directory property set to false") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs.getName(), "false");
    putfile->setAutoTerminatedRelationships(std::array{core::Relationship("failure", "description")});

    std::ofstream of(std::string(dir) + utils::file::FileUtils::get_separator() + "tstFile.ext");
    of << "tempFile";
    of.close();
    auto path = std::string(putfiledir) + utils::file::FileUtils::get_separator() + "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE_FALSE(utils::file::exists(putfiledir));
    REQUIRE_FALSE(utils::file::exists(path));
    std::string check = "PutFile commit put file operation to " + path + " failed because write failed";
    REQUIRE(LogTestController::getInstance().contains(check));
  }
}

#endif
