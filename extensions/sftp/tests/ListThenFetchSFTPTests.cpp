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
#undef NDEBUG
#include <cassert>
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
#include <sstream>
#include <algorithm>
#include <functional>
#include <iterator>
#include <random>

#include "TestBase.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "processors/FetchSFTP.h"
#include "processors/ListSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "processors/PutFile.h"
#include "tools/SFTPTestServer.h"

class ListThenFetchSFTPTestsFixture {
 public:
  ListThenFetchSFTPTestsFixture()
  : src_dir(strdup("/tmp/sftps.XXXXXX"))
  , dst_dir(strdup("/tmp/sftpd.XXXXXX")) {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::ListSFTP>();
    LogTestController::getInstance().setTrace<processors::FetchSFTP>();
    LogTestController::getInstance().setTrace<processors::PutFile>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    // Create temporary directories
    testController.createTempDirectory(src_dir);
    REQUIRE(src_dir != nullptr);
    testController.createTempDirectory(dst_dir);
    REQUIRE(dst_dir != nullptr);

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(src_dir));
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    plan = testController.createPlan();
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
    plan->setProperty(list_sftp, "Listing Strategy", processors::ListSFTP::LISTING_STRATEGY_TRACKING_TIMESTAMPS);
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
    plan->setProperty(list_sftp, "Target System Timestamp Precision", processors::ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT);
    plan->setProperty(list_sftp, "Minimum File Age", "0 sec");
    plan->setProperty(list_sftp, "Minimum File Size", "0 B");
    plan->setProperty(list_sftp, "Target System Timestamp Precision", "Seconds");
    plan->setProperty(list_sftp, "Remote Path", "nifi_test/");
    plan->setProperty(list_sftp, "State File", std::string(src_dir) + "/state");

    // Configure FetchSFTP processor
    plan->setProperty(fetch_sftp, "Hostname", "localhost");
    plan->setProperty(fetch_sftp, "Port", std::to_string(sftp_server->getPort()));
    plan->setProperty(fetch_sftp, "Username", "nifiuser");
    plan->setProperty(fetch_sftp, "Password", "nifipassword");
    plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_NONE);
    plan->setProperty(fetch_sftp, "Connection Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Data Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Strict Host Key Checking", "false");
    plan->setProperty(fetch_sftp, "Send Keep Alive On Timeout", "true");
    plan->setProperty(fetch_sftp, "Use Compression", "false");
    plan->setProperty(fetch_sftp, "Remote File", "${path}/${filename}");

    // Configure LogAttribute processor
    plan->setProperty(log_attribute, "FlowFiles To Log", "0");

    // Configure PutFile processor
    plan->setProperty(put_file, "Directory", std::string(dst_dir) + "/${path}");
    plan->setProperty(put_file, "Conflict Resolution Strategy", processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_FAIL);
    plan->setProperty(put_file, "Create Missing Directories", "true");
  }

  virtual ~ListThenFetchSFTPTestsFixture() {
    free(src_dir);
    free(dst_dir);
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content, uint64_t modification_timestamp = 0U) {
    std::fstream file;
    std::stringstream ss;
    ss << src_dir << "/vfs/" << relative_path;
    auto full_path = ss.str();
    std::deque<std::string> parent_dirs;
    std::string parent_dir = full_path;
    while ((parent_dir = utils::file::FileUtils::get_parent_path(parent_dir)) != "") {
      parent_dirs.push_front(parent_dir);
    }
    for (const auto& dir : parent_dirs) {
      utils::file::FileUtils::create_dir(dir);
    }
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
    if (modification_timestamp != 0U) {
      REQUIRE(true == utils::file::FileUtils::set_last_write_time(full_path, modification_timestamp));
    }
  }

  void createFileWithModificationTimeDiff(const std::string& relative_path, const std::string& content, int64_t modification_timediff = -300 /*5 minutes ago*/) {
    time_t now = time(nullptr);
    return createFile(relative_path, content, now + modification_timediff);
  }

  enum TestWhere {
    IN_DESTINATION,
    IN_SOURCE
  };

  void testFile(TestWhere where, const std::string& relative_path, const std::string& expected_content) {
    std::stringstream resultFile;
    if (where == IN_DESTINATION) {
      resultFile << dst_dir << "/" << relative_path;
    } else {
      resultFile << src_dir << "/vfs/" << relative_path;
#ifndef WIN32
      /* Workaround for mina-sshd setting the read file's permissions to 0000 */
      REQUIRE(0 == chmod(resultFile.str().c_str(), 0644));
#endif
    }
    std::ifstream file(resultFile.str());
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
    std::stringstream resultFile;
    if (where == IN_DESTINATION) {
      resultFile << dst_dir << "/" << relative_path;
    } else {
      resultFile << src_dir << "/vfs/" << relative_path;
#ifndef WIN32
      /* Workaround for mina-sshd setting the read file's permissions to 0000 */
      REQUIRE(-1 == chmod(resultFile.str().c_str(), 0644));
#endif
    }
    std::ifstream file(resultFile.str());
    REQUIRE(false == file.is_open());
    REQUIRE(false == file.good());
  }

 protected:
  char *src_dir;
  char *dst_dir;
  std::unique_ptr<SFTPTestServer> sftp_server;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
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
