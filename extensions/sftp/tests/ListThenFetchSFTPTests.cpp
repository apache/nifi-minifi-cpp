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

#include <sys/stat.h>
#include <cstring>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <algorithm>
#include <functional>
#include <iterator>
#include <random>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "utils/file/FileUtils.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "FlowController.h"
#include "unit/ProvenanceTestHelper.h"
#include "processors/FetchSFTP.h"
#include "processors/ListSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "tools/SFTPTestServer.h"

using namespace std::literals::chrono_literals;

class ListThenFetchSFTPTestsFixture {
 public:
  ListThenFetchSFTPTestsFixture() {
    LogTestController::getInstance().reset();
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<minifi::processors::ListSFTP>();
    LogTestController::getInstance().setTrace<minifi::processors::FetchSFTP>();
    LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    REQUIRE_FALSE(src_dir.empty());
    REQUIRE_FALSE(dst_dir.empty());
    REQUIRE(plan);

    // Start SFTP server
    sftp_server = std::make_unique<SFTPTestServer>(src_dir);
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    list_sftp = plan->addProcessor(
        "ListSFTP",
        "ListSFTP");
    fetch_sftp = plan->addProcessor(
        "FetchSFTP",
        "FetchSFTP",
        core::Relationship("success", "d"),
        true);
    log_attribute = plan->addProcessor("LogAttribute",
        "LogAttribute",
        { core::Relationship("success", "d"),
          core::Relationship("comms.failure", "d"),
          core::Relationship("not.found", "d"),
          core::Relationship("permission.denied", "d") },
          true);
    put_file = plan->addProcessor("PutFile",
         "PutFile",
         core::Relationship("success", "d"),
         true);

    // Configure ListSFTP processor
    plan->setProperty(list_sftp, "Listing Strategy", minifi::processors::ListSFTP::LISTING_STRATEGY_TRACKING_TIMESTAMPS);
    plan->setProperty(list_sftp, "Hostname", "localhost");
    plan->setProperty(list_sftp, "Port", std::to_string(sftp_server->getPort()));
    plan->setProperty(list_sftp, "Username", "nifiuser");
    plan->setProperty(list_sftp, "Password", "nifipassword");
    plan->setProperty(list_sftp, "Search Recursively", "false");
    plan->setProperty(list_sftp, "Follow symlink", "false");
    plan->setProperty(list_sftp, "Ignore Dotted Files", "false");
    plan->setProperty(list_sftp, "Strict Host Key Checking", "false");
    plan->setProperty(list_sftp, "Connection Timeout", "30 sec");
    plan->setProperty(list_sftp, "Data Timeout", "30 sec");
    plan->setProperty(list_sftp, "Send Keep Alive On Timeout", "true");
    plan->setProperty(list_sftp, "Target System Timestamp Precision", minifi::processors::ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT);
    plan->setProperty(list_sftp, "Minimum File Age", "0 sec");
    plan->setProperty(list_sftp, "Minimum File Size", "0 B");
    plan->setProperty(list_sftp, "Target System Timestamp Precision", "Seconds");
    plan->setProperty(list_sftp, "Remote Path", "nifi_test/");
    plan->setProperty(list_sftp, "State File", (src_dir / "state").string());

    // Configure FetchSFTP processor
    plan->setProperty(fetch_sftp, "Hostname", "localhost");
    plan->setProperty(fetch_sftp, "Port", std::to_string(sftp_server->getPort()));
    plan->setProperty(fetch_sftp, "Username", "nifiuser");
    plan->setProperty(fetch_sftp, "Password", "nifipassword");
    plan->setProperty(fetch_sftp, "Completion Strategy", minifi::processors::FetchSFTP::COMPLETION_STRATEGY_NONE);
    plan->setProperty(fetch_sftp, "Connection Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Data Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Strict Host Key Checking", "false");
    plan->setProperty(fetch_sftp, "Send Keep Alive On Timeout", "true");
    plan->setProperty(fetch_sftp, "Use Compression", "false");
    plan->setProperty(fetch_sftp, "Remote File", "${path}/${filename}");

    // Configure LogAttribute processor
    plan->setProperty(log_attribute, "FlowFiles To Log", "0");

    // Configure PutFile processor
    plan->setProperty(put_file, "Directory", (dst_dir / "${path}").string());
    plan->setProperty(put_file, "Conflict Resolution Strategy", magic_enum::enum_name(minifi::processors::PutFile::FileExistsResolutionStrategy::fail));
    plan->setProperty(put_file, "Create Missing Directories", "true");
  }

  ListThenFetchSFTPTestsFixture(ListThenFetchSFTPTestsFixture&&) = delete;
  ListThenFetchSFTPTestsFixture(const ListThenFetchSFTPTestsFixture&) = delete;
  ListThenFetchSFTPTestsFixture& operator=(ListThenFetchSFTPTestsFixture&&) = delete;
  ListThenFetchSFTPTestsFixture& operator=(const ListThenFetchSFTPTestsFixture&) = delete;

  virtual ~ListThenFetchSFTPTestsFixture() = default;

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content, const std::optional<std::chrono::file_clock::time_point>& modification_time) const {
    std::fstream file;
    const auto full_path = src_dir / "vfs" / relative_path;
    std::filesystem::create_directories(full_path.parent_path());
    file.open(full_path, std::ios::out);
    file << content;
    file.close();
    if (modification_time.has_value()) {
      REQUIRE(utils::file::set_last_write_time(full_path, modification_time.value()));
    }
  }

  void createFileWithModificationTimeDiff(const std::string& relative_path, const std::string& content, std::chrono::seconds modification_timediff = -5min) const {
    createFile(relative_path, content, std::chrono::file_clock::now() + modification_timediff);
  }

  enum TestWhere {
    IN_DESTINATION,
    IN_SOURCE
  };

  void testFile(TestWhere where, const std::filesystem::path& relative_path, std::string_view expected_content) const {
    std::filesystem::path expected_path = where == IN_DESTINATION ? dst_dir / relative_path : src_dir / "vfs" / relative_path;
    REQUIRE(std::filesystem::exists(expected_path));
    std::filesystem::permissions(expected_path, static_cast<std::filesystem::perms>(0644));;

    std::ifstream file(expected_path);
    REQUIRE(file.good());
    std::stringstream content;
    std::vector<char> buffer(1024U);
    while (file) {
      file.read(buffer.data(), gsl::narrow<std::streamsize>(buffer.size()));
      content << std::string(buffer.data(), file.gcount());
    }
    CHECK(expected_content == content.str());
  }

 protected:
  TestController testController;
  std::filesystem::path src_dir = testController.createTempDirectory();
  std::filesystem::path dst_dir = testController.createTempDirectory();
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::unique_ptr<SFTPTestServer> sftp_server;
  core::Processor* list_sftp;
  core::Processor* fetch_sftp;
  core::Processor* log_attribute;
  core::Processor* put_file;
};

TEST_CASE_METHOD(ListThenFetchSFTPTestsFixture, "ListSFTP then FetchSFTP one file", "[ListThenFetchSFTP][basic]") {
  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  testFile(IN_SOURCE, "nifi_test/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");
}

TEST_CASE_METHOD(ListThenFetchSFTPTestsFixture, "ListSFTP then FetchSFTP two files", "[ListThenFetchSFTP][basic]") {
  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2");

  /* ListSFTP */
  plan->runNextProcessor();

  /* FetchSFTP */
  plan->runNextProcessor();
  plan->runCurrentProcessor();

  /* LogAttribute */
  plan->runNextProcessor();

  /* PutFile */
  plan->runNextProcessor();
  plan->runCurrentProcessor();

  testFile(IN_SOURCE, "nifi_test/file1.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/file1.ext", "Test content 1");
  testFile(IN_SOURCE, "nifi_test/file2.ext", "Test content 2");
  testFile(IN_DESTINATION, "nifi_test/file2.ext", "Test content 2");
}
