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
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "ManipulateArchive.h"

const char TEST_ARCHIVE_NAME[] = "manipulate_test_archive.tar";
const int NUM_FILES = 3;
const char* FILE_NAMES[NUM_FILES] = {"first", "middle", "last"};
const char* FILE_CONTENT[NUM_FILES] = {"Test file 1\n", "Test file 2\n", "Test file 3\n"};

const char* MODIFY_SRC = FILE_NAMES[0];
const char* ORDER_ANCHOR = FILE_NAMES[1];
const char* MODIFY_DEST = "modified";

typedef std::map<std::string, std::string> PROP_MAP_T;

bool run_archive_test(OrderedTestArchive input_archive, OrderedTestArchive output_archive, PROP_MAP_T properties, bool check_attributes = true) {
    TestController testController;
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::FocusArchiveEntry>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::UnfocusArchiveEntry>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::processors::ManipulateArchive>();
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

    REQUIRE(testController.createTempDirectory(dir1) != nullptr);
    std::shared_ptr<core::Processor> getfile = plan->addProcessor("GetFile", "getfileCreate2");
    plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::Directory.getName(), dir1);
    plan->setProperty(getfile, org::apache::nifi::minifi::processors::GetFile::KeepSourceFile.getName(), "true");

    std::shared_ptr<core::Processor> maprocessor = plan->addProcessor("ManipulateArchive", "testManipulateArchive", core::Relationship("success", "description"), true);

    for (auto kv : properties) {
      plan->setProperty(maprocessor, kv.first, kv.second);
    }

    REQUIRE(testController.createTempDirectory(dir2) != nullptr);
    std::shared_ptr<core::Processor> putfile2 = plan->addProcessor("PutFile", "PutFile2", core::Relationship("success", "description"), true);
    plan->setProperty(putfile2, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), dir2);
    plan->setProperty(putfile2, org::apache::nifi::minifi::processors::PutFile::ConflictResolution.getName(),
                      org::apache::nifi::minifi::processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_REPLACE);

    std::stringstream ss1;
    ss1 << dir1 << "/" << TEST_ARCHIVE_NAME;
    std::string archive_path_1 = ss1.str();

    build_test_archive(archive_path_1, input_archive);
    REQUIRE(check_archive_contents(archive_path_1, input_archive, true));

    plan->runNextProcessor();  // GetFile
    plan->runNextProcessor();  // ManipulateArchive
    plan->runNextProcessor();  // PutFile 2 (manipulated)

    std::stringstream ss2;
    ss2 << dir2 << "/" << TEST_ARCHIVE_NAME;
    std::string output_path = ss2.str();
    return check_archive_contents(output_path, output_archive, check_attributes);
}

bool run_archive_test(TAE_MAP_T input_map, TAE_MAP_T output_map, PROP_MAP_T properties, bool check_attributes = true) {
  OrderedTestArchive input_archive, output_archive;

  // An empty vector is treated as "ignore order"
  input_archive.order = output_archive.order = FN_VEC_T();
  input_archive.map = input_map;
  output_archive.map = output_map;
  return run_archive_test(input_archive, output_archive, properties, check_attributes);
}

TEST_CASE("Test creation of ManipulateArchive", "[manipulatearchiveCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::ManipulateArchive>("processorname");
  REQUIRE(processor->getName() == "processorname");
  utils::Identifier processoruuid;
  REQUIRE(true == processor->getUUID(processoruuid));
}

TEST_CASE("Test ManipulateArchive Touch", "[testManipulateArchiveTouch]") {
    TAE_MAP_T test_archive_map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_TOUCH}
    };

    // The other attributes aren't checked, so we can leave them uninitialized
    TestArchiveEntry touched_entry;
    touched_entry.name = MODIFY_DEST;
    touched_entry.content = "";
    touched_entry.size = 0;
    touched_entry.type = AE_IFREG;

    // Copy original map and append touched entry
    TAE_MAP_T mod_archive_map(test_archive_map);
    mod_archive_map[MODIFY_DEST] = touched_entry;

    REQUIRE(run_archive_test(test_archive_map, mod_archive_map, properties, false));
}

TEST_CASE("Test ManipulateArchive Copy", "[testManipulateArchiveCopy]") {
    TAE_MAP_T test_archive_map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_COPY}
    };

    TAE_MAP_T mod_archive_map(test_archive_map);
    mod_archive_map[MODIFY_DEST] = test_archive_map[MODIFY_SRC];

    REQUIRE(run_archive_test(test_archive_map, mod_archive_map, properties));
}

TEST_CASE("Test ManipulateArchive Move", "[testManipulateArchiveMove]") {
    TAE_MAP_T test_archive_map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_MOVE}
    };

    TAE_MAP_T mod_archive_map(test_archive_map);

    mod_archive_map[MODIFY_DEST] = test_archive_map[MODIFY_SRC];
    mod_archive_map[MODIFY_DEST].name = MODIFY_DEST;

    auto it = mod_archive_map.find(MODIFY_SRC);
    mod_archive_map.erase(it);

    REQUIRE(run_archive_test(test_archive_map, mod_archive_map, properties));
}

TEST_CASE("Test ManipulateArchive Remove", "[testManipulateArchiveRemove]") {
    TAE_MAP_T test_archive_map = build_test_archive_map(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_REMOVE}
    };

    TAE_MAP_T mod_archive_map(test_archive_map);

    auto it = mod_archive_map.find(MODIFY_SRC);
    mod_archive_map.erase(it);

    REQUIRE(run_archive_test(test_archive_map, mod_archive_map, properties));
}

TEST_CASE("Test ManipulateArchive Ordered Touch (before)", "[testManipulateArchiveOrderedTouchBefore]") {
    OrderedTestArchive test_archive = build_ordered_test_archive(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_TOUCH},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Before.getName(), ORDER_ANCHOR}
    };

    // The other attributes aren't checked, so we can leave them uninitialized
    TestArchiveEntry touched_entry;
    touched_entry.name = MODIFY_DEST;
    touched_entry.content = "";
    touched_entry.size = 0;
    touched_entry.type = AE_IFREG;

    // Copy original map and append touched entry
    OrderedTestArchive mod_archive = test_archive;
    mod_archive.map[MODIFY_DEST] = touched_entry;

    auto it = std::find(mod_archive.order.begin(), mod_archive.order.end(), ORDER_ANCHOR);
    mod_archive.order.insert(it, MODIFY_DEST);

    REQUIRE(run_archive_test(test_archive, mod_archive, properties, false));
}

TEST_CASE("Test ManipulateArchive Ordered Copy (before)", "[testManipulateArchiveOrderedCopyBefore]") {
    OrderedTestArchive test_archive = build_ordered_test_archive(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_COPY},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Before.getName(), ORDER_ANCHOR}
    };

    OrderedTestArchive mod_archive = test_archive;
    mod_archive.map[MODIFY_DEST] = test_archive.map[MODIFY_SRC];
    auto it = std::find(mod_archive.order.begin(), mod_archive.order.end(), ORDER_ANCHOR);
    mod_archive.order.insert(it, MODIFY_DEST);

    REQUIRE(run_archive_test(test_archive, mod_archive, properties));
}

TEST_CASE("Test ManipulateArchive Ordered Move (before)", "[testManipulateArchiveOrderedMoveBefore]") {
    OrderedTestArchive test_archive = build_ordered_test_archive(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_MOVE},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Before.getName(), ORDER_ANCHOR}
    };

    OrderedTestArchive mod_archive = test_archive;

    // Update map
    mod_archive.map[MODIFY_DEST] = test_archive.map[MODIFY_SRC];
    mod_archive.map[MODIFY_DEST].name = MODIFY_DEST;
    auto m_it = mod_archive.map.find(MODIFY_SRC);
    mod_archive.map.erase(m_it);

    // Update order
    auto o_it = std::find(mod_archive.order.begin(), mod_archive.order.end(), MODIFY_SRC);
    mod_archive.order.erase(o_it);
    o_it = std::find(mod_archive.order.begin(), mod_archive.order.end(), ORDER_ANCHOR);
    mod_archive.order.insert(o_it, MODIFY_DEST);

    REQUIRE(run_archive_test(test_archive, mod_archive, properties));
}

TEST_CASE("Test ManipulateArchive Ordered Touch (after)", "[testManipulateArchiveOrderedTouchAfter]") {
    OrderedTestArchive test_archive = build_ordered_test_archive(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_TOUCH},
      {org::apache::nifi::minifi::processors::ManipulateArchive::After.getName(), ORDER_ANCHOR}
    };

    // The other attributes aren't checked, so we can leave them uninitialized
    TestArchiveEntry touched_entry;
    touched_entry.name = MODIFY_DEST;
    touched_entry.content = "";
    touched_entry.size = 0;
    touched_entry.type = AE_IFREG;

    // Copy original map and append touched entry
    OrderedTestArchive mod_archive = test_archive;
    mod_archive.map[MODIFY_DEST] = touched_entry;

    auto it = std::find(mod_archive.order.begin(), mod_archive.order.end(), ORDER_ANCHOR);
    it++;
    mod_archive.order.insert(it, MODIFY_DEST);

    REQUIRE(run_archive_test(test_archive, mod_archive, properties, false));
}

TEST_CASE("Test ManipulateArchive Ordered Copy (after)", "[testManipulateArchiveOrderedCopyAfter]") {
    OrderedTestArchive test_archive = build_ordered_test_archive(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_COPY},
      {org::apache::nifi::minifi::processors::ManipulateArchive::After.getName(), ORDER_ANCHOR}
    };

    OrderedTestArchive mod_archive = test_archive;
    mod_archive.map[MODIFY_DEST] = test_archive.map[MODIFY_SRC];
    auto it = std::find(mod_archive.order.begin(), mod_archive.order.end(), ORDER_ANCHOR);
    it++;
    mod_archive.order.insert(it, MODIFY_DEST);

    REQUIRE(run_archive_test(test_archive, mod_archive, properties));
}

TEST_CASE("Test ManipulateArchive Ordered Move (after)", "[testManipulateArchiveOrderedMoveAfter]") {
    OrderedTestArchive test_archive = build_ordered_test_archive(NUM_FILES, FILE_NAMES, FILE_CONTENT);

    PROP_MAP_T properties {
      {org::apache::nifi::minifi::processors::ManipulateArchive::Target.getName(), MODIFY_SRC},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Destination.getName(), MODIFY_DEST},
      {org::apache::nifi::minifi::processors::ManipulateArchive::Operation.getName(),
       org::apache::nifi::minifi::processors::ManipulateArchive::OPERATION_MOVE},
      {org::apache::nifi::minifi::processors::ManipulateArchive::After.getName(), ORDER_ANCHOR}
    };

    OrderedTestArchive mod_archive = test_archive;

    // Update map
    mod_archive.map[MODIFY_DEST] = test_archive.map[MODIFY_SRC];
    mod_archive.map[MODIFY_DEST].name = MODIFY_DEST;
    auto m_it = mod_archive.map.find(MODIFY_SRC);
    mod_archive.map.erase(m_it);

    // Update order
    auto o_it = std::find(mod_archive.order.begin(), mod_archive.order.end(), MODIFY_SRC);
    mod_archive.order.erase(o_it);
    o_it = std::find(mod_archive.order.begin(), mod_archive.order.end(), ORDER_ANCHOR);
    o_it++;
    mod_archive.order.insert(o_it, MODIFY_DEST);

    REQUIRE(run_archive_test(test_archive, mod_archive, properties));
}
