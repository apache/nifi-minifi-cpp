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
#ifndef WIN32
#include <unistd.h>
#endif

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
#include "processors/ListSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "tools/SFTPTestServer.h"
#include "utils/TestUtils.h"

class ListSFTPTestsFixture {
 public:
  explicit ListSFTPTestsFixture(const std::shared_ptr<minifi::Configure>& configuration = nullptr) {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::ListSFTP>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    REQUIRE_FALSE(src_dir.empty());

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(src_dir));
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    createPlan(nullptr, configuration);
  }

  virtual ~ListSFTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  void createPlan(utils::Identifier* list_sftp_uuid = nullptr, const std::shared_ptr<minifi::Configure>& configuration = nullptr) {
    const std::string state_dir = plan == nullptr ? testController.createTempDirectory() : plan->getStateDir();

    log_attribute.reset();
    list_sftp.reset();
    plan.reset();

    plan = testController.createPlan(configuration, state_dir.c_str());
    if (list_sftp_uuid == nullptr) {
      list_sftp = plan->addProcessor(
          "ListSFTP",
          "ListSFTP");
    } else {
      list_sftp = plan->addProcessor(
          "ListSFTP",
          *list_sftp_uuid,
          "ListSFTP",
          {core::Relationship("success", "d")});
    }
    log_attribute = plan->addProcessor("LogAttribute",
                                       "LogAttribute",
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

    // Configure LogAttribute processor
    plan->setProperty(log_attribute, "FlowFiles To Log", "0");
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

 protected:
  TestController testController;
  std::string src_dir = testController.createTempDirectory();
  std::shared_ptr<TestPlan> plan;
  std::unique_ptr<SFTPTestServer> sftp_server;
  std::shared_ptr<core::Processor> list_sftp;
  std::shared_ptr<core::Processor> log_attribute;
};

class PersistentListSFTPTestsFixture : public ListSFTPTestsFixture {
 public:
  PersistentListSFTPTestsFixture() :
    ListSFTPTestsFixture(std::make_shared<minifi::Configure>()) {
  }
};

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list one file", "[ListSFTP][basic]") {
  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP public key authentication", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(list_sftp, "Private Key Path", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"));
  plan->setProperty(list_sftp, "Private Key Passphrase", "privatekeypassword");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Successfully authenticated with publickey"));

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list non-existing dir", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Remote Path", "nifi_test2/");

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Failed to open remote directory \"nifi_test2\", error: LIBSSH2_FX_NO_SUCH_FILE"));
  REQUIRE(LogTestController::getInstance().contains("There are no files to list. Yielding."));
}

#ifndef WIN32
TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list non-readable dir", "[ListSFTP][basic]") {
  if (getuid() == 0) {
    std::cerr << "!!!! This test does NOT work as root. Exiting. !!!!" << std::endl;
    return;
  }
  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");
  REQUIRE(0 == chmod((src_dir + "/vfs/nifi_test").c_str(), 0000));

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Failed to open remote directory \"nifi_test\", error: LIBSSH2_FX_PERMISSION_DENIED"));
  REQUIRE(LogTestController::getInstance().contains("There are no files to list. Yielding."));
}
#endif

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list one file writes attributes", "[ListSFTP][basic]") {
  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  auto file = src_dir + "/vfs/nifi_test/tstFile.ext";
  auto mtime = utils::file::FileUtils::last_write_time(file);
  std::string mtime_str;
  REQUIRE(true == utils::timeutils::getDateTimeStr(mtime, mtime_str));
  uint64_t uid, gid;
  REQUIRE(true == utils::file::FileUtils::get_uid_gid(file, uid, gid));
  uint32_t permissions;
  REQUIRE(true == utils::file::FileUtils::get_permissions(file, permissions));
  std::stringstream permissions_ss;
  permissions_ss << std::setfill('0') << std::setw(4) << std::oct << permissions;

  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.listing.user value:nifiuser"));
  REQUIRE(LogTestController::getInstance().contains("key:file.owner value:" + std::to_string(uid)));
  REQUIRE(LogTestController::getInstance().contains("key:file.group value:" + std::to_string(gid)));
  REQUIRE(LogTestController::getInstance().contains("key:file.permissions value:" + permissions_ss.str()));
  REQUIRE(LogTestController::getInstance().contains("key:file.size value:14"));
  REQUIRE(LogTestController::getInstance().contains("key:file.lastModifiedTime value:" + mtime_str));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list two files", "[ListSFTP][basic]") {
  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list two files one in a subdir no recursion", "[ListSFTP][basic]") {
  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  createFileWithModificationTimeDiff("nifi_test/subdir/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list two files one in a subdir with recursion", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Search Recursively", "true");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  createFileWithModificationTimeDiff("nifi_test/subdir/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Minimum File Age too young", "[ListSFTP][file-age]") {
  plan->setProperty(list_sftp, "Minimum File Age", "2 hours");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("Ignoring \"nifi_test/tstFile.ext\" because it is younger than the Minimum File Age"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Maximum File Age too old", "[ListSFTP][file-age]") {
  plan->setProperty(list_sftp, "Maximum File Age", "1 min");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("Ignoring \"nifi_test/tstFile.ext\" because it is older than the Maximum File Age"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Minimum File Size too small", "[ListSFTP][file-size]") {
  plan->setProperty(list_sftp, "Minimum File Size", "1 MB");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);
  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("Ignoring \"nifi_test/tstFile.ext\" because it is smaller than the Minimum File Size: 14 B < 1048576 B"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Maximum File Size too large", "[ListSFTP][file-size]") {
  plan->setProperty(list_sftp, "Maximum File Size", "4 B");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);
  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("Ignoring \"nifi_test/tstFile.ext\" because it is larger than the Maximum File Size: 14 B > 4 B"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP File Filter Regex", "[ListSFTP][file-filter-regex]") {
  plan->setProperty(list_sftp, "File Filter Regex", "^.*2.*$");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Ignoring \"nifi_test/file1.ext\" because it did not match the File Filter Regex \"^.*2.*$\""));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Path Filter Regex", "[ListSFTP][path-filter-regex]") {
  plan->setProperty(list_sftp, "Search Recursively", "true");
  plan->setProperty(list_sftp, "Path Filter Regex", "^.*foobar.*$");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  createFileWithModificationTimeDiff("nifi_test/foobar/file2.ext", "Test content 2");
  createFileWithModificationTimeDiff("nifi_test/notbar/file3.ext", "Test with longer content 3");

  testController.runSession(plan, true);

  /* file1.ext is in the root */
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  /* file2.ext is in a matching subdirectory */
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
  /* file3.ext is in a non-matching subdirectory */
  REQUIRE(LogTestController::getInstance().contains("Not recursing into \"nifi_test/notbar\" because it did not match the Path Filter Regex \"^.*foobar.*$\""));
  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:file3.ext"));
}

#ifndef WIN32
TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Follow symlink false file symlink", "[ListSFTP][follow-symlink]") {
  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  auto file1 = src_dir + "/vfs/nifi_test/file1.ext";
  auto file2 = src_dir + "/vfs/nifi_test/file2.ext";
  REQUIRE(0 == symlink(file1.c_str(), file2.c_str()));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("Skipping non-regular, non-directory file \"nifi_test/file2.ext\""));
}
#endif

#ifndef WIN32
TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Follow symlink true file symlink", "[ListSFTP][follow-symlink]") {
  plan->setProperty(list_sftp, "Follow symlink", "true");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");
  auto file1 = src_dir + "/vfs/nifi_test/file1.ext";
  auto file2 = src_dir + "/vfs/nifi_test/file2.ext";
  REQUIRE(0 == symlink(file1.c_str(), file2.c_str()));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}
#endif

#ifndef WIN32
TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Follow symlink false directory symlink", "[ListSFTP][follow-symlink]") {
  plan->setProperty(list_sftp, "Search Recursively", "true");

  createFileWithModificationTimeDiff("nifi_test/dir1/file1.ext", "Test content 1");
  auto dir1 = src_dir + "/vfs/nifi_test/dir1";
  auto dir2 = src_dir + "/vfs/nifi_test/dir2";
  REQUIRE(0 == symlink(dir1.c_str(), dir2.c_str()));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("Skipping non-regular, non-directory file \"nifi_test/dir2\""));
}
#endif

#ifndef WIN32
TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Follow symlink true directory symlink", "[ListSFTP][follow-symlink]") {
  plan->setProperty(list_sftp, "Search Recursively", "true");
  plan->setProperty(list_sftp, "Follow symlink", "true");

  createFileWithModificationTimeDiff("nifi_test/dir1/file1.ext", "Test content 1");
  auto dir1 = src_dir + "/vfs/nifi_test/dir1";
  auto dir2 = src_dir + "/vfs/nifi_test/dir2";
  REQUIRE(0 == symlink(dir1.c_str(), dir2.c_str()));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/dir1"));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/dir2"));
}
#endif

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Timestamps one file", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("The latest listed entry timestamp is the same as the last listed entry timestamp"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Timestamps one file one new file", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Timestamps one file one older file", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -360 /* 6 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Skipping \"nifi_test/file2.ext\", because it is not new."));
  REQUIRE(LogTestController::getInstance().contains("The latest listed entry timestamp is the same as the last listed entry timestamp"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Timestamps one file another file with the same timestamp", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  auto file = src_dir + "/vfs/nifi_test/file1.ext";
  auto mtime = utils::file::FileUtils::last_write_time(file);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  /* We must sleep to avoid triggering the listing lag. */
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  createFile("nifi_test/file2.ext", "Test content 2", mtime);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Timestamps one file timestamp updated", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  auto file = src_dir + "/vfs/nifi_test/file1.ext";
  auto mtime = utils::file::FileUtils::last_write_time(file);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  REQUIRE(true == utils::file::FileUtils::set_last_write_time(file, mtime + 1));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  /* We must sleep to avoid triggering the listing lag. */
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("and all files for that timestamp has been processed. Yielding."));
}

TEST_CASE_METHOD(PersistentListSFTPTestsFixture, "ListSFTP Tracking Timestamps restore state", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  std::unordered_map<std::string, std::string> state;
  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Timestamps" == state.at("listing_strategy"));
  REQUIRE("nifi_test/file1.ext" == state.at("id.0"));
  REQUIRE(!state.at("listing.timestamp").empty());
  REQUIRE(!state.at("processed.timestamp").empty());

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  utils::Identifier list_sftp_uuid = list_sftp->getUUID();
  REQUIRE(list_sftp_uuid);
  createPlan(&list_sftp_uuid, std::make_shared<minifi::Configure>());
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Timestamps" == state.at("listing_strategy"));
  REQUIRE("nifi_test/file2.ext" == state.at("id.0"));
  REQUIRE(!state.at("listing.timestamp").empty());
  REQUIRE(!state.at("processed.timestamp").empty());

  REQUIRE(LogTestController::getInstance().contains("Successfully loaded state"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
  REQUIRE(!LogTestController::getInstance().contains("key:filename value:file1.ext", std::chrono::seconds(0)));
}

TEST_CASE_METHOD(PersistentListSFTPTestsFixture, "ListSFTP Tracking Timestamps restore state changed configuration", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  std::unordered_map<std::string, std::string> state;
  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Timestamps" == state.at("listing_strategy"));

  utils::Identifier list_sftp_uuid = list_sftp->getUUID();
  REQUIRE(list_sftp_uuid);
  createPlan(&list_sftp_uuid, std::make_shared<minifi::Configure>());
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");
  plan->setProperty(list_sftp, "Remote Path", "/nifi_test");
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("/nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Timestamps" == state.at("listing_strategy"));

  REQUIRE(LogTestController::getInstance().contains("Processor state was persisted with different settings than the current ones, ignoring. "
                                                    "Listing Strategy: \"Tracking Timestamps\" vs. \"Tracking Timestamps\", "
                                                    "Hostname: \"localhost\" vs. \"localhost\", "
                                                    "Username: \"nifiuser\" vs. \"nifiuser\", "
                                                    "Remote Path: \"nifi_test\" vs. \"/nifi_test\""));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Timestamps changed configuration", "[ListSFTP][tracking-timestamps]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Timestamps");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
  plan->setProperty(list_sftp, "Remote Path", "/nifi_test");

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Processor state was persisted with different settings than the current ones, ignoring. "
                                                    "Listing Strategy: \"Tracking Timestamps\" vs. \"Tracking Timestamps\", "
                                                    "Hostname: \"localhost\" vs. \"localhost\", "
                                                    "Username: \"nifiuser\" vs. \"nifiuser\", "
                                                    "Remote Path: \"nifi_test\" vs. \"/nifi_test\""));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Skipping file \"nifi_test/tstFile.ext\" because it has not changed"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file one new file", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file one older file in tracking window", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -360 /* 6 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("Skipping file \"nifi_test/file1.ext\" because it has not changed"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file one older file outside tracking window", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -6 * 3600 /* 6 hours ago */);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Skipping file \"nifi_test/file1.ext\" because it has not changed"));
  REQUIRE(LogTestController::getInstance().contains("Skipping \"nifi_test/file2.ext\" because it has an older timestamp than the minimum timestamp to list"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file another file with the same timestamp", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  auto file = src_dir + "/vfs/nifi_test/file1.ext";
  auto mtime = utils::file::FileUtils::last_write_time(file);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFile("nifi_test/file2.ext", "Test content 2", mtime);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Skipping file \"nifi_test/file1.ext\" because it has not changed"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file timestamp updated", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  auto file = src_dir + "/vfs/nifi_test/file1.ext";
  auto mtime = utils::file::FileUtils::last_write_time(file);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  REQUIRE(true == utils::file::FileUtils::set_last_write_time(file, mtime + 1));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Found file \"nifi_test/file1.ext\" with newer timestamp"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Skipping file \"nifi_test/file1.ext\" because it has not changed"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities one file size changes", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  auto file = src_dir + "/vfs/nifi_test/file1.ext";
  auto mtime = utils::file::FileUtils::last_write_time(file);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFile("nifi_test/file1.ext", "Longer test content 1", mtime);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Found file \"nifi_test/file1.ext\" with different size: 14 -> 21"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("Skipping file \"nifi_test/file1.ext\" because it has not changed"));
}

TEST_CASE_METHOD(PersistentListSFTPTestsFixture, "ListSFTP Tracking Entities restore state", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  utils::Identifier list_sftp_uuid = list_sftp->getUUID();
  REQUIRE(list_sftp_uuid);
  createPlan(&list_sftp_uuid, std::make_shared<minifi::Configure>());
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  std::unordered_map<std::string, std::string> state;
  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Entities" == state.at("listing_strategy"));
  REQUIRE("nifi_test/file1.ext" == state.at("entity.0.name"));
  REQUIRE("14" == state.at("entity.0.size"));
  REQUIRE(!state.at("entity.0.timestamp").empty());

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Entities" == state.at("listing_strategy"));
  if (state.at("entity.0.name") == "nifi_test/file1.ext") {
    REQUIRE("nifi_test/file1.ext" == state.at("entity.0.name"));
    REQUIRE("nifi_test/file2.ext" == state.at("entity.1.name"));
  } else {
    REQUIRE("nifi_test/file2.ext" == state.at("entity.0.name"));
    REQUIRE("nifi_test/file1.ext" == state.at("entity.1.name"));
  }
  REQUIRE("14" == state.at("entity.0.size"));
  REQUIRE(!state.at("entity.0.timestamp").empty());
  REQUIRE("14" == state.at("entity.1.size"));
  REQUIRE(!state.at("entity.1.timestamp").empty());

  REQUIRE(LogTestController::getInstance().contains("Successfully loaded state"));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
  REQUIRE(!LogTestController::getInstance().contains("key:filename value:file1.ext", std::chrono::seconds(0)));
}

TEST_CASE_METHOD(PersistentListSFTPTestsFixture, "ListSFTP Tracking Entities restore state changed configuration", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  std::unordered_map<std::string, std::string> state;
  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Entities" == state.at("listing_strategy"));

  utils::Identifier list_sftp_uuid = list_sftp->getUUID();
  REQUIRE(list_sftp_uuid);
  createPlan(&list_sftp_uuid, std::make_shared<minifi::Configure>());
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");
  plan->setProperty(list_sftp, "Remote Path", "/nifi_test");
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(plan->getStateManagerProvider()->getCoreComponentStateManager(*list_sftp)->get(state));
  REQUIRE("localhost" == state.at("hostname"));
  REQUIRE("nifiuser" == state.at("username"));
  REQUIRE("/nifi_test" == state.at("remote_path"));
  REQUIRE("Tracking Entities" == state.at("listing_strategy"));

  REQUIRE(LogTestController::getInstance().contains("Processor state was persisted with different settings than the current ones, ignoring. "
                                                    "Listing Strategy: \"Tracking Entities\" vs. \"Tracking Entities\", "
                                                    "Hostname: \"localhost\" vs. \"localhost\", "
                                                    "Username: \"nifiuser\" vs. \"nifiuser\", "
                                                    "Remote Path: \"nifi_test\" vs. \"/nifi_test\""));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities changed configuration", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));

  plan->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
  plan->setProperty(list_sftp, "Remote Path", "/nifi_test");

  createFileWithModificationTimeDiff("nifi_test/file2.ext", "Test content 2", -240 /* 4 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Processor state was persisted with different settings than the current ones, ignoring. "
                                                    "Listing Strategy: \"Tracking Entities\" vs. \"Tracking Entities\", "
                                                    "Hostname: \"localhost\" vs. \"localhost\", "
                                                    "Username: \"nifiuser\" vs. \"nifiuser\", "
                                                    "Remote Path: \"nifi_test\" vs. \"/nifi_test\""));
  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file2.ext"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities Initial Listing Target Tracking Time Window entity outside window", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");
  plan->setProperty(list_sftp, "Entity Tracking Initial Listing Target", processors::ListSFTP::ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW);
  plan->setProperty(list_sftp, "Entity Tracking Time Window", "10 minutes");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1", -20*60 /* 20 minutes ago */);

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Skipping \"nifi_test/file1.ext\" because it has an older timestamp than the minimum timestamp to list"));
  REQUIRE(false == LogTestController::getInstance().contains("from ListSFTP to relationship success"));
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP Tracking Entities Initial Listing Target Tracking Time Window entity inside window", "[ListSFTP][tracking-entities]") {
  plan->setProperty(list_sftp, "Listing Strategy", "Tracking Entities");
  plan->setProperty(list_sftp, "Entity Tracking Initial Listing Target", processors::ListSFTP::ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW);
  plan->setProperty(list_sftp, "Entity Tracking Time Window", "10 minutes");

  createFileWithModificationTimeDiff("nifi_test/file1.ext", "Test content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from ListSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:file1.ext"));
}
