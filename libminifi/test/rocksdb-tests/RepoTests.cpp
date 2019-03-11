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
#include "../TestBase.h"
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <map>
#include "../unit/ProvenanceTestHelper.h"
#include "provenance/Provenance.h"
#include "FlowFileRecord.h"
#include "core/Core.h"
#include "FlowFileRepository.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "properties/Configure.h"

TEST_CASE("Test Repo Names", "[TestFFR1]") {
  auto repoA = minifi::core::createRepository("FlowFileRepository", false, "flowfile");
  REQUIRE("flowfile" == repoA->getName());

  auto repoB = minifi::core::createRepository("ProvenanceRepository", false, "provenance");
  REQUIRE("provenance" == repoB->getName());
}

TEST_CASE("Test Repo Empty Value Attribute", "[TestFFR1]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0, 1);

  repository->initialize(std::make_shared<minifi::Configure>());

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  minifi::FlowFileRecord record(repository, content_repo);

  record.addAttribute("keyA", "");

  REQUIRE(true == record.Serialize());

  utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY, true);

  repository->stop();
}

TEST_CASE("Test Repo Empty Key Attribute ", "[TestFFR2]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0, 1);

  repository->initialize(std::make_shared<minifi::Configure>());
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  minifi::FlowFileRecord record(repository, content_repo);

  record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

  REQUIRE(true == record.Serialize());

  utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY, true);

  repository->stop();
}

TEST_CASE("Test Repo Key Attribute Verify ", "[TestFFR3]") {
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  char *dir = testController.createTempDirectory(format);
  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0, 1);

  repository->initialize(std::make_shared<org::apache::nifi::minifi::Configure>());

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();
  minifi::FlowFileRecord record(repository, content_repo);

  minifi::FlowFileRecord record2(repository, content_repo);

  std::string uuid = record.getUUIDStr();

  record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  record.addAttribute("keyB", "");

  record.addAttribute("", "");

  record.updateAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd2");

  record.addAttribute("", "sdgsdg");

  REQUIRE(true == record.Serialize());

  repository->stop();

  record2.DeSerialize(uuid);

  std::string value;
  REQUIRE(true == record2.getAttribute("", value));

  REQUIRE("hasdgasdgjsdgasgdsgsadaskgasd2" == value);

  REQUIRE(false == record2.getAttribute("key", value));
  REQUIRE(true == record2.getAttribute("keyA", value));
  REQUIRE("hasdgasdgjsdgasgdsgsadaskgasd" == value);

  REQUIRE(true == record2.getAttribute("keyB", value));
  REQUIRE("" == value);

  utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY, true);
}

TEST_CASE("Test Delete Content ", "[TestFFR4]") {
  TestController testController;
  char format[] = "/tmp/testRepo.XXXXXX";
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setDebug<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setDebug<core::repository::FlowFileRepository>();

  char *dir = testController.createTempDirectory(format);

  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0, 1);

  std::map<std::string, std::string> attributes;

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  repository->initialize(std::make_shared<minifi::Configure>());

  repository->loadComponent(content_repo);

  std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ss.str(), content_repo);

  minifi::FlowFileRecord record(repository, content_repo, attributes, claim);

  record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

  record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

  REQUIRE(true == record.Serialize());

  claim->decreaseFlowFileRecordOwnedCount();

  claim->decreaseFlowFileRecordOwnedCount();

  repository->Delete(record.getUUIDStr());

  repository->flush();

  repository->stop();

  std::ifstream fileopen(ss.str());
  REQUIRE(false == fileopen.good());

  utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY, true);

  LogTestController::getInstance().reset();
}

TEST_CASE("Test Validate Checkpoint ", "[TestFFR5]") {
  TestController testController;
  utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY, true);
  char format[] = "/tmp/testRepo.XXXXXX";
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();

  char *dir = testController.createTempDirectory(format);

  std::shared_ptr<core::repository::FlowFileRepository> repository = std::make_shared<core::repository::FlowFileRepository>("ff", dir, 0, 0, 1);

  std::map<std::string, std::string> attributes;

  std::fstream file;
  std::stringstream ss;
  ss << dir << "/" << "tstFile.ext";
  file.open(ss.str(), std::ios::out);
  file << "tempFile";
  file.close();

  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();

  repository->initialize(std::make_shared<minifi::Configure>());

  repository->loadComponent(content_repo);

  std::shared_ptr<minifi::ResourceClaim> claim = std::make_shared<minifi::ResourceClaim>(ss.str(), content_repo);
  {
    minifi::FlowFileRecord record(repository, content_repo, attributes, claim);

    record.addAttribute("keyA", "hasdgasdgjsdgasgdsgsadaskgasd");

    record.addAttribute("", "hasdgasdgjsdgasgdsgsadaskgasd");

    REQUIRE(true == record.Serialize());

    repository->flush();

    repository->stop();

    repository->loadComponent(content_repo);

    repository->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    repository->stop();
    claim = nullptr;
    // sleep for 100 ms to let the delete work.

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::ifstream fileopen(ss.str());

  REQUIRE(true == fileopen.fail());

  utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY, true);

  LogTestController::getInstance().reset();
}

