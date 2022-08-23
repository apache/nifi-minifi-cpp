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

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <iterator>

#include "TestBase.h"
#include "Catch.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/ProcessGroup.h"
#include "FlowController.h"
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
    sftp_server = std::make_unique<SFTPTestServer>(src_dir.string());
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
    plan->setProperty(put_file, "Conflict Resolution Strategy", minifi::processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_FAIL);
    plan->setProperty(put_file, "Create Missing Directories", "true");
  }

  virtual ~ListThenFetchSFTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content, std::optional<std::filesystem::file_time_type> modification_time) {
    const auto full_path = src_dir / "vfs" / relative_path;
    std::filesystem::create_directories(full_path.parent_path());

    std::fstream file;
    file.open(full_path, std::ios::out);
    file << content;
    file.close();
    if (modification_time.has_value()) {
      std::error_code ec;
      utils::file::set_last_write_time(full_path.string(), modification_time.value());
      REQUIRE(ec.value() == 0);
    }
  }

  void createFileWithModificationTimeDiff(const std::string& relative_path, const std::string& content, std::chrono::seconds modification_timediff = -5min) {
    return createFile(relative_path, content, std::chrono::file_clock::now() + modification_timediff);
  }

  enum TestWhere {
    IN_DESTINATION,
    IN_SOURCE
  };

  void testFile(TestWhere where, const std::string& relative_path, const std::string& expected_content) {
    std::filesystem::path result_file;
    if (where == IN_DESTINATION) {
      result_file = dst_dir / relative_path;
    } else {
      result_file = src_dir / "vfs" / relative_path;
#ifndef WIN32
      /* Workaround for mina-sshd setting the read file's permissions to 0000 */
      REQUIRE(0 == chmod(result_file.string().c_str(), 0644));
#endif
    }
    std::ifstream file(result_file);
    REQUIRE(true == file.good());
    std::stringstream content;
    std::vector<char> buffer(1024U);
    while (file) {
      file.read(buffer.data(), buffer.size());
      content << std::string(buffer.data(), file.gcount());
    }
    REQUIRE(expected_content == content.str());
  }

  void testFileNotExists(TestWhere where, const std::string& relative_path) {
    std::filesystem::path result_file;
    if (where == IN_DESTINATION) {
      result_file = dst_dir / relative_path;
    } else {
      result_file = src_dir / "vfs" / relative_path;
#ifndef WIN32
      /* Workaround for mina-sshd setting the read file's permissions to 0000 */
      REQUIRE(-1 == chmod(result_file.string().c_str(), 0644));
#endif
    }
    std::ifstream file(result_file);
    REQUIRE(false == file.is_open());
    REQUIRE(false == file.good());
  }

 protected:
  TestController testController;
  std::filesystem::path src_dir{testController.createTempDirectory()};
  std::filesystem::path dst_dir{testController.createTempDirectory()};
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::unique_ptr<SFTPTestServer> sftp_server;
  std::shared_ptr<core::Processor> list_sftp;
  std::shared_ptr<core::Processor> fetch_sftp;
  std::shared_ptr<core::Processor> log_attribute;
  std::shared_ptr<core::Processor> put_file;
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
