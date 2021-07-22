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
#include "processors/FetchSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
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
    LogTestController::getInstance().setDebug<processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::FetchSFTP>();
    LogTestController::getInstance().setTrace<processors::PutFile>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    REQUIRE_FALSE(src_dir.empty());
    REQUIRE_FALSE(dst_dir.empty());
    REQUIRE(plan);

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(src_dir));
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
    plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_NONE);
    plan->setProperty(fetch_sftp, "Connection Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Data Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Strict Host Key Checking", "false");
    plan->setProperty(fetch_sftp, "Send Keep Alive On Timeout", "true");
    plan->setProperty(fetch_sftp, "Use Compression", "false");

    // Configure PutFile processor
    plan->setProperty(put_file, "Directory", dst_dir + "/${path}");
    plan->setProperty(put_file, "Conflict Resolution Strategy", processors::PutFile::CONFLICT_RESOLUTION_STRATEGY_FAIL);
    plan->setProperty(put_file, "Create Missing Directories", "true");
  }

  virtual ~FetchSFTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content) {
    std::fstream file;
    std::stringstream ss;
    ss << src_dir << "/vfs/" << relative_path;
    utils::file::create_dir(utils::file::get_parent_path(ss.str()));
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
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
  TestController testController;
  std::string src_dir = testController.createTempDirectory();
  std::string dst_dir = testController.createTempDirectory();
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
  plan->setProperty(fetch_sftp, "Private Key Path", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"));
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
  REQUIRE(0 == chmod((src_dir + "/vfs/nifi_test/tstFile.ext").c_str(), 0000));

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
  plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_DELETE_FILE);

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
  plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_DELETE_FILE);

  createFile("nifi_test/tstFile.ext", "Test content 1");
  /* By making the parent directory non-writable we make it impossible do delete the source file */
  REQUIRE(0 == chmod((src_dir + "/vfs/nifi_test").c_str(), 0500));

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
}
#endif

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP Completion Strategy Move File success", "[FetchSFTP][completion-strategy]") {
  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");
  plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE);
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
  plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE);
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
  plan->setProperty(update_attribute, "attr_Hostname", "localhost", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Port", std::to_string(sftp_server->getPort()), true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Username", "nifiuser", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Password", "nifipassword", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Private Key Path",
    utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"), true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Private Key Passphrase", "privatekeypassword", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Remote File", "nifi_test/tstFile.ext", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Move Destination Directory", "nifi_done/", true /*dynamic*/);

  plan->setProperty(fetch_sftp, "Hostname", "${'attr_Hostname'}");
  plan->setProperty(fetch_sftp, "Port", "${'attr_Port'}");
  plan->setProperty(fetch_sftp, "Username", "${'attr_Username'}");
  plan->setProperty(fetch_sftp, "Password", "${'attr_Password'}");
  plan->setProperty(fetch_sftp, "Private Key Path", "${'attr_Private Key Path'}");
  plan->setProperty(fetch_sftp, "Private Key Passphrase", "${'attr_Private Key Passphrase'}");
  plan->setProperty(fetch_sftp, "Remote File", "${'attr_Remote File'}");
  plan->setProperty(fetch_sftp, "Move Destination Directory", "${'attr_Move Destination Directory'}");

  plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_MOVE_FILE);
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
