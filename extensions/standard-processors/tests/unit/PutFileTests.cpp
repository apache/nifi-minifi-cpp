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
#include <fstream>

#include "utils/file/FileUtils.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "unit/TestUtils.h"
#include "processors/LogAttribute.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Core.h"
#include "minifi-cpp/core/FlowFile.h"
#include "core/Processor.h"
#include "core/reporting/SiteToSiteProvenanceReportingTask.h"
#include "minifi-cpp/Exception.h"

TEST_CASE("Test Creation of PutFile", "[getfileCreate]") {
  TestController testController;
  auto processor = minifi::test::utils::make_processor<org::apache::nifi::minifi::processors::PutFile>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("PutFileTest", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, putfiledir.string());

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  std::filesystem::remove(path);

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + (dir / "").string()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(path).good());
  auto moved_path = putfiledir / "tstFile.ext";

  REQUIRE(true == std::ifstream(moved_path).good());

  file.open(moved_path, std::ios::in);
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

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("failure", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto put_file_dir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, put_file_dir.string());

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  auto moved_path = put_file_dir / "tstFile.ext";
  file.open(moved_path, std::ios::out);
  file << "tempFile";
  file.close();

  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  std::filesystem::remove(path);

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + (dir / "").string()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(path).good());
  REQUIRE(true == std::ifstream(moved_path).good());

  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileTestFileExistsIgnore", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto put_file_dir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, put_file_dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::ConflictResolution, "ignore");

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  auto moved_path = put_file_dir / "tstFile.ext";
  file.open(moved_path, std::ios::out);
  file << "tempFile";
  file.close();
  auto file_mod_time = utils::file::last_write_time(moved_path);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  std::filesystem::remove(path);

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + (dir / "").string() ));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(path).good());
  REQUIRE(true == std::ifstream(moved_path).good());
  REQUIRE(file_mod_time == utils::file::last_write_time(moved_path));
  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileTestFileExistsReplace", "[getfileputpfile]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", { core::Relationship("success", "d"), core::Relationship("failure", "d") }, true);

  const auto dir = testController.createTempDirectory();
  const auto put_file_dir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, put_file_dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::ConflictResolution, "replace");

  testController.runSession(plan, false);

  auto records = plan->getProvenanceRecords();
  std::shared_ptr<core::FlowFile> record = plan->getCurrentFlowFile();
  REQUIRE(record == nullptr);
  REQUIRE(records.empty());

  std::fstream file;
  auto path = dir / "tstFile.ext";
  file.open(path, std::ios::out);
  file << "tempFile";
  file.close();

  auto moved_path = put_file_dir / "tstFile.ext";
  file.open(moved_path, std::ios::out);
  file << "tempFile";
  file.close();
  auto file_mod_time = utils::file::last_write_time(moved_path);

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  plan->reset();

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  testController.runSession(plan, false);

  std::filesystem::remove(path);

  REQUIRE(true == LogTestController::getInstance().contains("key:absolute.path value:" + (dir / "").string()));
  REQUIRE(true == LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(true == LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));
  // verify that the fle was moved
  REQUIRE(false == std::ifstream(path).good());
  REQUIRE(true == std::ifstream(moved_path).good());
#ifndef WIN32
  REQUIRE(file_mod_time != utils::file::last_write_time(moved_path));
#endif
  LogTestController::getInstance().reset();
}

TEST_CASE("PutFileMaxFileCountTest", "[getfileputpfilemaxcount]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto getfile = plan->addProcessor("GetFile", "getfileCreate");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  plan->addProcessor("LogAttribute", "logattribute", { core::Relationship("success", "d"), core::Relationship("failure", "d") }, true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::BatchSize, "1");
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, putfiledir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::MaxDestFiles, "1");



  for (int i = 0; i < 2; ++i) {
    auto path = dir / ("tstFile" + std::to_string(i) + ".ext");
    std::fstream file;
    file.open(path, std::ios::out);
    file << "tempFile";
    file.close();
  }

  plan->reset();

  testController.runSession(plan);

  plan->reset();

  testController.runSession(plan);


  REQUIRE(LogTestController::getInstance().contains("key:absolute.path value:" + (dir / "").string()));
  REQUIRE(LogTestController::getInstance().contains("Size:8 Offset:0"));
  REQUIRE(LogTestController::getInstance().contains("key:path value:" + (std::filesystem::path(".") / "").string()));

  // Only 1 of the 2 files should make it to the target dir
  // Non-deterministic, so let's just count them
  int files_in_dir = 0;

  for (int i = 0; i < 2; ++i) {
    auto path = putfiledir / ("tstFile" + std::to_string(i) + ".ext");
    std::ifstream file(path);
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

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory();

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, putfiledir.string());

  std::ofstream of(dir / "tstFile.ext");
  of.close();

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Put

  std::ifstream is(putfiledir / "tstFile.ext", std::ifstream::binary);

  REQUIRE(is.is_open());
  is.seekg(0, is.end);
  CHECK(is.tellg() == 0);
}

#ifndef WIN32
TEST_CASE("TestPutFilePermissions", "[PutFilePermissions]") {
  TestController testController;

  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();

  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");

  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);

  const auto dir = testController.createTempDirectory();
  const auto putfiledir = testController.createTempDirectory() / "test_dir";

  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, putfiledir.string());
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Permissions, "644");
  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::DirectoryPermissions, "0777");

  std::fstream file;
  file.open(dir / "tstFile.ext", std::ios::out);
  file << "tempFile";
  file.close();

  plan->runNextProcessor();  // Get
  plan->runNextProcessor();  // Put

  auto path = putfiledir / "tstFile.ext";
  uint32_t perms = 0;
  CHECK(utils::file::FileUtils::get_permissions(path, perms));
  CHECK(perms == 0644);
  CHECK(utils::file::FileUtils::get_permissions(putfiledir, perms));
  CHECK(perms == 0777);
}

TEST_CASE("PutFileCreateDirectoryTest", "[PutFileProperties]") {
  TestController testController;
  LogTestController::getInstance().setDebug<minifi::processors::GetFile>();
  LogTestController::getInstance().setDebug<TestPlan>();
  LogTestController::getInstance().setDebug<minifi::processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  auto getfile = plan->addProcessor("GetFile", "getfileCreate2");
  auto putfile = plan->addProcessor("PutFile", "putfile", core::Relationship("success", "description"), true);
  plan->addProcessor("LogAttribute", "logattribute", core::Relationship("success", "description"), true);

  // Define Directory
  auto dir = testController.createTempDirectory();
  // Defining a subdirectory
  auto putfiledir = testController.createTempDirectory() / "test_dir";

  plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::Directory, putfiledir.string());
  plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory, dir.string());

  SECTION("with an empty file and create directory property set to true") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs, "true");

    std::ofstream of(dir / "tstFile.ext");
    of.close();
    auto path = putfiledir / "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE(utils::file::exists(putfiledir));
    REQUIRE(utils::file::exists(path));
  }

  SECTION("with an empty file and create directory property set to false") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs, "false");
    putfile->setAutoTerminatedRelationships(std::array{core::Relationship("failure", "description")});

    std::ofstream of(dir / "tstFile.ext");
    of.close();
    auto path = putfiledir / "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE_FALSE(utils::file::exists(putfiledir));
    REQUIRE_FALSE(utils::file::exists(path));
  }

  SECTION("with a non-empty file and create directory property set to true") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs, "true");

    std::ofstream of(dir / "tstFile.ext");
    of << "tempFile";
    of.close();
    auto path = putfiledir / "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE(utils::file::exists(putfiledir));
    REQUIRE(utils::file::exists(path));
  }

  SECTION("with a non-empty file and create directory property set to false") {
    plan->setProperty(putfile, org::apache::nifi::minifi::processors::PutFile::CreateDirs, "false");
    putfile->setAutoTerminatedRelationships(std::array{core::Relationship("failure", "description")});

    std::ofstream of(dir / "tstFile.ext");
    of << "tempFile";
    of.close();
    auto path = putfiledir / "tstFile.ext";

    plan->runNextProcessor();
    plan->runNextProcessor();

    REQUIRE_FALSE(utils::file::exists(putfiledir));
    REQUIRE_FALSE(utils::file::exists(path));
  }
}

#endif
