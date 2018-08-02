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
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <memory>
#include <utility>

#include "../TestBase.h"
#include "ArchiveTests.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/LogAttribute.h"
#include "FocusArchiveEntry.h"
#include "UnfocusArchiveEntry.h"

#include <archive.h>
#include <archive_entry.h>

const char TEST_ARCHIVE_NAME[] = "focus_test_archive.tar";
const int NUM_FILES = 2;
const char* FILE_NAMES[NUM_FILES] = {"file1", "file2"};
const char* FILE_CONTENT[NUM_FILES] = {"Test file 1\n", "Test file 2\n"};

const char* FOCUSED_FILE = FILE_NAMES[0];
const char* FOCUSED_CONTENT = FILE_CONTENT[0];


TEST_CASE("Test Creation of FocusArchiveEntry", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::FocusArchiveEntry>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test Creation of UnfocusArchiveEntry", "[getfileCreate]") {
    TestController testController;
    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::UnfocusArchiveEntry>("processorname");
    REQUIRE(processor->getName() == "processorname");
    utils::Identifier processoruuid;
    REQUIRE(true == processor->getUUID(processoruuid));
}

TEST_CASE("FocusArchive", "[testFocusArchive]") {
    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::FocusArchiveEntry>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::UnfocusArchiveEntry>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::PutFile>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::GetFile>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::repository::VolatileContentRepository>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::Connection>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::Connectable>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::core::FlowFile>();

    std::shared_ptr<TestPlan> plan = testController.createPlan();
    std::shared_ptr<TestRepository> repo = std::make_shared<TestRepository>();

    char dir1[] = "/tmp/gt.XXXXXX";
    char dir2[] = "/tmp/gt.XXXXXX";
    char dir3[] = "/tmp/gt.XXXXXX";

    REQUIRE(testController.createTempDirectory(dir1) != nullptr);
    std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");
    plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir1);
    plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");

    std::shared_ptr<core::Processor> fprocessor = plan->addProcessor("FocusArchiveEntry", "focusarchiveCreate", core::Relationship("success", "description"), true);
    plan->setProperty(fprocessor, org::apache::nifi::minifi::processors::FocusArchiveEntry::Path.getName(), FOCUSED_FILE);

    REQUIRE(testController.createTempDirectory(dir2) != nullptr);
    std::shared_ptr<core::Processor> putfile1 = plan->addProcessor("PutFile", "PutFile1", core::Relationship("success", "description"), true);
    plan->setProperty(putfile1, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir2);
    plan->setProperty(putfile1, org::apache::nifi::minifi::processors::PutFile::ConflictResolution.getName(),
                      org::apache::nifi::minifi::processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);

    std::shared_ptr<core::Processor> ufprocessor = plan->addProcessor("UnfocusArchiveEntry", "unfocusarchiveCreate", core::Relationship("success", "description"), true);

    REQUIRE(testController.createTempDirectory(dir3) != nullptr);
    std::shared_ptr<core::Processor> putfile2 = plan->addProcessor("PutFile", "PutFile2", core::Relationship("success", "description"), true);
    plan->setProperty(putfile2, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir3);
    plan->setProperty(putfile2, org::apache::nifi::minifi::processors::PutFile::ConflictResolution.getName(),
                      org::apache::nifi::minifi::processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);

    std::stringstream ss1;
    ss1 << dir1 << "/" << TEST_ARCHIVE_NAME;
    std::string archive_path_1 = ss1.str();

    TAE_MAP_T test_archive_map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);
    build_test_archive(archive_path_1, test_archive_map);

    REQUIRE(check_archive_contents(archive_path_1, test_archive_map));

    plan->runNextProcessor();  // GetFile
    plan->runNextProcessor();  // FocusArchive
    plan->runNextProcessor();  // PutFile 1 (focused)

    std::stringstream ss2;
    ss2 << dir2 << "/" << FOCUSED_FILE;
    std::ifstream ifs(ss2.str().c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    std::ifstream::pos_type size = ifs.tellg();
    int64_t bufsize {size};
    ifs.seekg(0, std::ios::beg);
    char *content = new char[bufsize];
    ifs.read(content, bufsize);

    REQUIRE(size == strlen(FOCUSED_CONTENT));
    REQUIRE(memcmp(content, FOCUSED_CONTENT, size) == 0);

    plan->runNextProcessor();  // UnfocusArchive
    plan->runNextProcessor();  // PutFile 2 (unfocused)

    std::stringstream ss3;
    ss3 << dir3 << "/" << TEST_ARCHIVE_NAME;
    std::string archive_path_2 = ss3.str();
    REQUIRE(check_archive_contents(archive_path_2, test_archive_map));
}
