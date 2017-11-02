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
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "FocusArchiveEntry.h"
#include "UnfocusArchiveEntry.h"

#include <archive.h>
#include <archive_entry.h>

typedef struct {
    const char* content;
    std::string name;
    mode_t type;
    mode_t perms;
    uid_t uid;
    gid_t gid;
    time_t mtime;
    uint32_t mtime_nsec;
    size_t size;
} TestArchiveEntry;

typedef std::map<std::string, TestArchiveEntry> TAE_MAP_T;

const char TEST_ARCHIVE_NAME[] = "focus_test_archive.tar";
const int NUM_FILES = 2;
const char* FILE_NAMES[NUM_FILES] = {"file1", "file2"};
const char* FILE_CONTENT[NUM_FILES] = {"Test file 1\n", "Test file 2\n"};

const char* FOCUSED_FILE = FILE_NAMES[0];
const char* FOCUSED_CONTENT = FILE_CONTENT[0];

TAE_MAP_T build_test_archive_map() {
    TAE_MAP_T test_entries;

    for (int i = 0; i < NUM_FILES; i++) {
        std::string name {FILE_NAMES[i]};
        TestArchiveEntry entry;

        entry.name = name;
        entry.content = FILE_CONTENT[i];
        entry.size = strlen(FILE_CONTENT[i]);
        entry.type = AE_IFREG;
        entry.perms = 0765;
        entry.uid = 12; entry.gid = 34;
        entry.mtime = time(nullptr);
        entry.mtime_nsec = 0;

        test_entries[name] = entry;
    }

    return test_entries;
}

void build_test_archive(std::string path, TAE_MAP_T entries) {
    std::cout << "Creating " << path << std::endl;
    archive * test_archive = archive_write_new();

    archive_write_set_format_ustar(test_archive);
    archive_write_open_filename(test_archive, path.c_str());
    struct archive_entry* entry = archive_entry_new();

    for (auto &kvp : entries) {
        std::string name = kvp.first;
        TestArchiveEntry test_entry = kvp.second;

        std::cout << "Adding entry: " << name << std::endl;

        archive_entry_set_filetype(entry, test_entry.type);
        archive_entry_set_pathname(entry, test_entry.name.c_str());
        archive_entry_set_size(entry, test_entry.size);
        archive_entry_set_perm(entry, test_entry.perms);
        archive_entry_set_uid(entry, test_entry.uid);
        archive_entry_set_gid(entry, test_entry.gid);
        archive_entry_set_mtime(entry, test_entry.mtime, test_entry.mtime_nsec);

        archive_write_header(test_archive, entry);
        archive_write_data(test_archive, test_entry.content, test_entry.size);

        archive_entry_clear(entry);
    }

    archive_entry_free(entry);
    archive_write_close(test_archive);
}

bool check_archive_contents(std::string path, TAE_MAP_T entries) {
    std::set<std::string> read_names;
    std::set<std::string> extra_names;
    bool ok = true;
    struct archive *a = archive_read_new();
    struct archive_entry *entry;

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_filename(a, path.c_str(), 16384);

    if (r != ARCHIVE_OK) {
        std::cout << "Unable to open archive " << path << " for checking!" << std::endl;
        return false;
    }

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name {archive_entry_pathname(entry)};
        auto it = entries.find(name);
        if (it == entries.end()) {
            extra_names.insert(name);
        } else {
            read_names.insert(name);
            TestArchiveEntry test_entry = it->second;
            size_t size = archive_entry_size(entry);

            std::cout << "Checking archive entry: " << name << std::endl;

            REQUIRE(size == test_entry.size);

            if (size > 0) {
                int rlen, nlen = 0;
                const char* buf[size];
                bool read_ok = true;

                for (;;) {
                  rlen = archive_read_data(a, buf, size);
                  nlen += rlen;
                  if (rlen == 0) break;
                  if (rlen < 0) {
                    std::cout << "FAIL: Negative size read?" << std::endl;
                    read_ok = false;
                    break;
                  }
                }

                if (read_ok) {
                    REQUIRE(nlen == size);
                    REQUIRE(memcmp(buf, test_entry.content, size) == 0);
                }
            }

            REQUIRE(archive_entry_filetype(entry) == test_entry.type);
            REQUIRE(archive_entry_uid(entry) == test_entry.uid);
            REQUIRE(archive_entry_gid(entry) == test_entry.gid);
            REQUIRE(archive_entry_perm(entry) == test_entry.perms);
            REQUIRE(archive_entry_mtime(entry) == test_entry.mtime);
        }
    }

    archive_read_close(a);
    archive_read_free(a);

    if (!extra_names.empty()) {
        ok = false;
        std::cout << "Extra files found: ";
        for (std::string filename : extra_names) std::cout << filename << " ";
        std::cout << std::endl;
    }

    REQUIRE(extra_names.empty());

    std::set<std::string> test_file_entries;
    std::transform(entries.begin(), entries.end(),
                   std::inserter(test_file_entries, test_file_entries.end()),
                   [](std::pair<std::string, TestArchiveEntry> p){ return p.first; });
    REQUIRE(read_names == test_file_entries);

    return ok;
}

TEST_CASE("Test Creation of FocusArchiveEntry", "[getfileCreate]") {
  TestController testController;
  std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::FocusArchiveEntry>("processorname");
  REQUIRE(processor->getName() == "processorname");
}

TEST_CASE("Test Creation of UnfocusArchiveEntry", "[getfileCreate]") {
    TestController testController;
    std::shared_ptr<core::Processor> processor = std::make_shared<org::apache::nifi::minifi::processors::UnfocusArchiveEntry>("processorname");
    REQUIRE(processor->getName() == "processorname");
    uuid_t processoruuid;
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

    TAE_MAP_T test_archive_map = build_test_archive_map();
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
