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

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "archive.h"
#include "archive_entry.h"
#include "util/ArchiveTests.h"
#include "FocusArchiveEntry.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "UnfocusArchiveEntry.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "repository/VolatileContentRepository.h"
#include "unit/ProvenanceTestHelper.h"

namespace org::apache::nifi::minifi::processors::test {

const std::string TEST_ARCHIVE_NAME = "focus_test_archive.tar";
constexpr int NUM_FILES = 2;
const char* FILE_NAMES[NUM_FILES] = {"file1", "file2"};  // NOLINT(cppcoreguidelines-avoid-c-arrays)
const char* FILE_CONTENT[NUM_FILES] = {"Test file 1\n", "Test file 2\n"};  // NOLINT(cppcoreguidelines-avoid-c-arrays)

const char* FOCUSED_FILE = FILE_NAMES[0];
const char* FOCUSED_CONTENT = FILE_CONTENT[0];

TEST_CASE("Test Creation of FocusArchiveEntry", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<FocusArchiveEntry>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test Creation of UnfocusArchiveEntry", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<UnfocusArchiveEntry>("processorname");
  REQUIRE(processor->getName() == "processorname");
  REQUIRE(processor->getUUID());
}

TEST_CASE("FocusArchive", "[testFocusArchive]") {
  TestController testController;
  LogTestController::getInstance().setTrace<FocusArchiveEntry>();
  LogTestController::getInstance().setTrace<UnfocusArchiveEntry>();
  LogTestController::getInstance().setTrace<PutFile>();
  LogTestController::getInstance().setTrace<GetFile>();
  LogTestController::getInstance().setTrace<LogAttribute>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();
  LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
  LogTestController::getInstance().setTrace<Connection>();
  LogTestController::getInstance().setTrace<core::Connectable>();
  LogTestController::getInstance().setTrace<core::FlowFile>();

  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

  auto dir1 = testController.createTempDirectory();
  auto dir2 = testController.createTempDirectory();
  auto dir3 = testController.createTempDirectory();

  REQUIRE(!dir1.empty());
  REQUIRE(!dir2.empty());
  REQUIRE(!dir3.empty());
  std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");
  plan->setProperty(getfile, GetFile::Directory, dir1.string());
  plan->setProperty(getfile, GetFile::KeepSourceFile, "true");

  std::shared_ptr<core::Processor> fprocessor = plan->addProcessor("FocusArchiveEntry", "focusarchiveCreate", core::Relationship("success", "description"), true);
  plan->setProperty(fprocessor, FocusArchiveEntry::Path, FOCUSED_FILE);

  std::shared_ptr<core::Processor> putfile1 = plan->addProcessor("PutFile", "PutFile1", core::Relationship("success", "description"), true);
  plan->setProperty(putfile1, PutFile::Directory, dir2.string());
  plan->setProperty(putfile1, PutFile::ConflictResolution, magic_enum::enum_name(PutFile::FileExistsResolutionStrategy::replace));

  std::shared_ptr<core::Processor> ufprocessor = plan->addProcessor("UnfocusArchiveEntry", "unfocusarchiveCreate", core::Relationship("success", "description"), true);

  std::shared_ptr<core::Processor> putfile2 = plan->addProcessor("PutFile", "PutFile2", core::Relationship("success", "description"), true);
  plan->setProperty(putfile2, PutFile::Directory, dir3.string());
  plan->setProperty(putfile2, PutFile::ConflictResolution, magic_enum::enum_name(PutFile::FileExistsResolutionStrategy::replace));

  auto archive_path_1 = dir1 / TEST_ARCHIVE_NAME;

  TAE_MAP_T test_archive_map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);
  build_test_archive(archive_path_1, test_archive_map);

  REQUIRE(check_archive_contents(archive_path_1, test_archive_map));

  plan->runNextProcessor();  // GetFile
  plan->runNextProcessor();  // FocusArchive
  plan->runNextProcessor();  // PutFile 1 (focused)

  std::ifstream ifs(dir2 / FOCUSED_FILE, std::ios::in | std::ios::binary | std::ios::ate);

  auto size = gsl::narrow<size_t>(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  auto content = std::vector<char>(size);
  ifs.read(content.data(), gsl::narrow<std::streamsize>(size));

  REQUIRE(size == strlen(FOCUSED_CONTENT));
  REQUIRE(memcmp(content.data(), FOCUSED_CONTENT, size) == 0);

  plan->runNextProcessor();  // UnfocusArchive
  plan->runNextProcessor();  // PutFile 2 (unfocused)

  auto archive_path_2 = dir3 / TEST_ARCHIVE_NAME;
  REQUIRE(check_archive_contents(archive_path_2, test_archive_map));
}

}  // namespace org::apache::nifi::minifi::processors::test
