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

#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <iterator>

#include "TestBase.h"
#include "Catch.h"
#include "utils/file/FileUtils.h"
#include "core/ProcessGroup.h"
#include "FlowController.h"
#include "processors/FetchSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/PutFile.h"
#include "tools/SFTPTestServer.h"

class FetchSFTPTestsFixture {
 public:
  FetchSFTPTestsFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
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
    generate_flow_file = plan->addProcessor(
        "GenerateFlowFile",
        "GenerateFlowFile");
    update_attribute = plan->addProcessor("UpdateAttribute",
         "UpdateAttribute",
         core::Relationship("success", "d"),
         true);
    fetch_sftp = plan->addProcessor(
        "FetchSFTP",
        "FetchSFTP",
        core::Relationship("success", "d"),
        true);
    plan->addProcessor("LogAttribute",
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

    // Configure GenerateFlowFile processor
    plan->setProperty(generate_flow_file, "File Size", "1B");

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

    // Configure PutFile processor
    plan->setProperty(put_file, "Directory", (dst_dir / "${path}").string());
    plan->setProperty(put_file, "Conflict Resolution Strategy", minifi::processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_FAIL);
    plan->setProperty(put_file, "Create Missing Directories", "true");
  }

  virtual ~FetchSFTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content) {
    const auto file_path = src_dir / "vfs" / relative_path;
    std::filesystem::create_directories(file_path.parent_path());

    std::fstream file;
    file.open(file_path, std::ios::out);
    file << content;
    file.close();
  }

  enum TestWhere {
    IN_DESTINATION,
    IN_SOURCE
  };

  void testFile(TestWhere where, const std::filesystem::path& relative_path, std::string_view expected_content) {
    std::filesystem::path expected_path = where == IN_DESTINATION ? dst_dir / relative_path : src_dir / "vfs" / relative_path;
    REQUIRE(std::filesystem::exists(expected_path));
    std::filesystem::permissions(expected_path, static_cast<std::filesystem::perms>(0644));

    std::ifstream file(expected_path);
    REQUIRE(file.good());
    std::stringstream content;
    std::vector<char> buffer(1024U);
    while (file) {
      file.read(buffer.data(), buffer.size());
      content << std::string(buffer.data(), file.gcount());
    }
    CHECK(expected_content == content.str());
  }

  void testFileNotExists(TestWhere where, const std::string& relative_path) {
    std::filesystem::path expected_path = where == IN_DESTINATION ? dst_dir / relative_path : src_dir / "vfs" / relative_path;
    CHECK(!std::filesystem::exists(expected_path));
  }

 protected:
  TestController testController;
  std::filesystem::path src_dir{testController.createTempDirectory()};
  std::filesystem::path dst_dir{testController.createTempDirectory()};
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::unique_ptr<SFTPTestServer> sftp_server;
  std::shared_ptr<core::Processor> generate_flow_file;
  std::shared_ptr<core::Processor> update_attribute;
  std::shared_ptr<core::Processor> fetch_sftp;
  std::shared_ptr<core::Processor> put_file;
};

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP fetch one file", "[FetchSFTP][basic]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  testFile(IN_SOURCE, "nifi_test/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("from FetchSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP public key authentication", "[FetchSFTP][basic]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(fetch_sftp, "Private Key Path", (get_sftp_test_dir() / "resources" / "id_rsa").generic_string());
  plan->setProperty(fetch_sftp, "Private Key Passphrase", "privatekeypassword");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  testFile(IN_SOURCE, "nifi_test/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("Successfully authenticated with publickey"));

  REQUIRE(LogTestController::getInstance().contains("from FetchSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP fetch non-existing file", "[FetchSFTP][basic]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Failed to open remote file \"nifi_test/tstFile.ext\", error: LIBSSH2_FX_NO_SUCH_FILE"));
  REQUIRE(LogTestController::getInstance().contains("from FetchSFTP to relationship not.found"));
}

#ifndef WIN32
TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP fetch non-readable file", "[FetchSFTP][basic]") {
  if (getuid() == 0) {
    std::cerr << "!!!! This test does NOT work as root. Exiting. !!!!" << std::endl;
    return;
  }

  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");

  createFile("nifi_test/tstFile.ext", "Test content 1");
  std::filesystem::permissions(src_dir / "vfs" / "nifi_test" / "tstFile.ext", static_cast<std::filesystem::perms>(0000));

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Failed to open remote file \"nifi_test/tstFile.ext\", error: LIBSSH2_FX_PERMISSION_DENIED"));
  REQUIRE(LogTestController::getInstance().contains("from FetchSFTP to relationship permission.denied"));
}
#endif

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP fetch connection error", "[FetchSFTP][basic]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  /* Run it once normally to open the connection */
  testController.runSession(plan, true);
  plan->reset();

  /* Stop the server to create a connection error */
  sftp_server.reset();
  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Failed to open remote file \"nifi_test/tstFile.ext\" due to an underlying SSH error"));
  REQUIRE(LogTestController::getInstance().contains("from FetchSFTP to relationship comms.failure"));
}

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP Completion Strategy Delete File success", "[FetchSFTP][completion-strategy]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(fetch_sftp, "Completion Strategy", minifi::processors::FetchSFTP::COMPLETION_STRATEGY_DELETE_FILE);

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  testFileNotExists(IN_SOURCE, "nifi_test/tstFile.ext");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

#ifndef WIN32
TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP Completion Strategy Delete File fail", "[FetchSFTP][completion-strategy]") {
  if (getuid() == 0) {
    std::cerr << "!!!! This test does NOT work as root. Exiting. !!!!" << std::endl;
    return;
  }
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(fetch_sftp, "Completion Strategy", minifi::processors::FetchSFTP::COMPLETION_STRATEGY_DELETE_FILE);

  createFile("nifi_test/tstFile.ext", "Test content 1");
  /* By making the parent directory non-writable we make it impossible do delete the source file */
  std::filesystem::permissions(src_dir / "vfs" / "nifi_test", static_cast<std::filesystem::perms>(0500));

  testController.runSession(plan, true);

  /* We should succeed even if the completion strategy fails */
  testFile(IN_SOURCE, "nifi_test/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("Failed to remove remote file \"nifi_test/tstFile.ext\", error: LIBSSH2_FX_PERMISSION_DENIED"));
  REQUIRE(LogTestController::getInstance().contains("Completion Strategy is Delete File, but failed to delete remote file \"nifi_test/tstFile.ext\""));

  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
  std::filesystem::permissions(src_dir / "vfs" / "nifi_test", static_cast<std::filesystem::perms>(0755));
}
#endif

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP Completion Strategy Move File success", "[FetchSFTP][completion-strategy]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(fetch_sftp, "Completion Strategy", minifi::processors::FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE);
  plan->setProperty(fetch_sftp, "Move Destination Directory", "nifi_done/");
  plan->setProperty(fetch_sftp, "Create Directory", "true");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  testFileNotExists(IN_SOURCE, "nifi_test/tstFile.ext");
  testFile(IN_SOURCE, "nifi_done/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP Completion Strategy Move File fail", "[FetchSFTP][completion-strategy]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(fetch_sftp, "Completion Strategy", minifi::processors::FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE);
  plan->setProperty(fetch_sftp, "Move Destination Directory", "nifi_done/");

  /* The completion strategy should fail because the target directory does not exist and we don't create it */
  plan->setProperty(fetch_sftp, "Create Directory", "false");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  /* We should succeed even if the completion strategy fails */
  testFileNotExists(IN_SOURCE, "nifi_done/tstFile.ext");
  testFile(IN_SOURCE, "nifi_test/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("Failed to rename remote file \"nifi_test/tstFile.ext\" to \"nifi_done/tstFile.ext\", error: LIBSSH2_FX_NO_SUCH_FILE"));
  REQUIRE(LogTestController::getInstance().contains("Completion Strategy is Move File, but failed to move file \"nifi_test/tstFile.ext\" to \"nifi_done/tstFile.ext\""));

  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP expression language test", "[FetchSFTP]") {
  plan->setDynamicProperty(update_attribute, "attr_Hostname", "localhost");
  plan->setDynamicProperty(update_attribute, "attr_Port", std::to_string(sftp_server->getPort()));
  plan->setDynamicProperty(update_attribute, "attr_Username", "nifiuser");
  plan->setDynamicProperty(update_attribute, "attr_Password", "nifipassword");
  plan->setDynamicProperty(update_attribute, "attr_Private Key Path", (get_sftp_test_dir() / "resources" / "id_rsa").generic_string());
  plan->setDynamicProperty(update_attribute, "attr_Private Key Passphrase", "privatekeypassword");
  plan->setDynamicProperty(update_attribute, "attr_Remote File", "nifi_test/tstFile.ext");
  plan->setDynamicProperty(update_attribute, "attr_Move Destination Directory", "nifi_done/");

  plan->setProperty(fetch_sftp, "Hostname", "${'attr_Hostname'}");
  plan->setProperty(fetch_sftp, "Port", "${'attr_Port'}");
  plan->setProperty(fetch_sftp, "Username", "${'attr_Username'}");
  plan->setProperty(fetch_sftp, "Password", "${'attr_Password'}");
  plan->setProperty(fetch_sftp, "Private Key Path", "${'attr_Private Key Path'}");
  plan->setProperty(fetch_sftp, "Private Key Passphrase", "${'attr_Private Key Passphrase'}");
  plan->setProperty(fetch_sftp, "Remote File", "${'attr_Remote File'}");
  plan->setProperty(fetch_sftp, "Move Destination Directory", "${'attr_Move Destination Directory'}");

  plan->setProperty(fetch_sftp, "Completion Strategy", minifi::processors::FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE);
  plan->setProperty(fetch_sftp, "Create Directory", "true");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);

  testFileNotExists(IN_SOURCE, "nifi_test/tstFile.ext");
  testFile(IN_SOURCE, "nifi_done/tstFile.ext", "Test content 1");
  testFile(IN_DESTINATION, "nifi_test/tstFile.ext", "Test content 1");

  REQUIRE(LogTestController::getInstance().contains("Successfully authenticated with publickey"));
  REQUIRE(LogTestController::getInstance().contains("from FetchSFTP to relationship success"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.filename value:nifi_test/tstFile.ext"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.host value:localhost"));
  REQUIRE(LogTestController::getInstance().contains("key:sftp.remote.port value:" + std::to_string(sftp_server->getPort())));
  REQUIRE(LogTestController::getInstance().contains("key:path value:nifi_test/"));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:tstFile.ext"));
}
