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
#include "Exception.h"
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
#include "processors/PutSFTP.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/ExtractText.h"
#include "processors/UpdateAttribute.h"
#include "tools/SFTPTestServer.h"

constexpr const char* PUBLIC_KEY_AUTH_ERROR_MESSAGE = "Failed to authenticate with publickey, error: Unable to extract public key from private key file: Wrong passphrase or invalid/unrecognized private key file format";  // NOLINT(whitespace/line_length)

class PutSFTPTestsFixture {
 public:
  PutSFTPTestsFixture() {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::GetFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::PutSFTP>();
    LogTestController::getInstance().setTrace<processors::ExtractText>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    REQUIRE_FALSE(src_dir.empty());
    REQUIRE_FALSE(dst_dir.empty());
    REQUIRE(plan);

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(dst_dir));
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    get_file = plan->addProcessor(
        "GetFile",
        "GetFile");
    put = plan->addProcessor(
        "PutSFTP",
        "PutSFTP",
        core::Relationship("success", "d"),
        true);
    plan->addProcessor("LogAttribute",
        "LogAttribute",
        { core::Relationship("success", "d"),
          core::Relationship("reject", "d"),
          core::Relationship("failure", "d") },
          true);

    // Configure GetFile processor
    plan->setProperty(get_file, "Input Directory", src_dir);

    // Configure PutSFTP processor
    plan->setProperty(put, "Hostname", "localhost");
    plan->setProperty(put, "Port", std::to_string(sftp_server->getPort()));
    plan->setProperty(put, "Username", "nifiuser");
    plan->setProperty(put, "Password", "nifipassword");
    plan->setProperty(put, "Remote Path", "nifi_test/");
    plan->setProperty(put, "Create Directory", "true");
    plan->setProperty(put, "Batch Size", "2");
    plan->setProperty(put, "Connection Timeout", "30 sec");
    plan->setProperty(put, "Data Timeout", "30 sec");
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);
    plan->setProperty(put, "Strict Host Key Checking", "false");
    plan->setProperty(put, "Send Keep Alive On Timeout", "true");
    plan->setProperty(put, "Use Compression", "false");
    plan->setProperty(put, "Reject Zero-Byte Files", "true");
  }

  virtual ~PutSFTPTestsFixture() {
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string &dir, const std::string& relative_path, const std::string& content) {
    std::fstream file;
    std::stringstream ss;
    ss << dir << "/" << relative_path;
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
  }

  // Test target file
  void testFile(const std::string& relative_path, const std::string& expected_content) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
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

  void testFileNotExists(const std::string& relative_path) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    std::ifstream file(resultFile.str());
    REQUIRE(false == file.is_open());
    REQUIRE(false == file.good());
  }

  void testModificationTime(const std::string& relative_path, int64_t mtime) {
    gsl_Expects(mtime >= 0);
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    REQUIRE(gsl::narrow<uint64_t>(mtime) == utils::file::FileUtils::last_write_time(resultFile.str()));
  }

  void testPermissions(const std::string& relative_path, uint32_t expected_permissions) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    uint32_t permissions = 0U;
    REQUIRE(true == utils::file::FileUtils::get_permissions(resultFile.str(), permissions));
    REQUIRE(expected_permissions == permissions);
  }

  void testOwner(const std::string& relative_path, uint64_t expected_uid) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    uint64_t uid = 0U;
    uint64_t gid = 0U;
    REQUIRE(true == utils::file::FileUtils::get_uid_gid(resultFile.str(), uid, gid));
    REQUIRE(expected_uid == uid);
  }

  void testGroup(const std::string& relative_path, uint64_t expected_gid) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    uint64_t uid = 0U;
    uint64_t gid = 0U;
    REQUIRE(true == utils::file::FileUtils::get_uid_gid(resultFile.str(), uid, gid));
    REQUIRE(expected_gid == gid);
  }

  size_t directoryContentCount(const std::string& dir) {
    size_t count = 0U;
    utils::file::FileUtils::list_dir(dir, [&count](const std::string&, const std::string&) {
      count++;
      return true;
    }, testController.getLogger());
    return count;
  }

 protected:
  TestController testController;
  std::string src_dir = testController.createTempDirectory();
  std::string dst_dir = testController.createTempDirectory();
  std::shared_ptr<TestPlan> plan = testController.createPlan();
  std::unique_ptr<SFTPTestServer> sftp_server;
  std::shared_ptr<core::Processor> get_file;
  std::shared_ptr<core::Processor> put;
};

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP put one file", "[PutSFTP][basic]") {
  createFile(src_dir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile.ext", "tempFile");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP put two files", "[PutSFTP][basic]") {
  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile1.ext", "content 1");
  testFile("nifi_test/tstFile2.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP bad password", "[PutSFTP][authentication]") {
  plan->setProperty(put, "Password", "badpassword");
  createFile(src_dir, "tstFile.ext", "tempFile");

  try {
    testController.runSession(plan, true);
  } catch (std::exception &e) {
    std::string expected = minifi::Exception(minifi::PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow").what();
    REQUIRE(0 == std::string(e.what()).compare(0, expected.size(), expected));
  }

  REQUIRE(LogTestController::getInstance().contains("Failed to authenticate with password, error: Authentication failed (username/password)"));
  REQUIRE(LogTestController::getInstance().contains("Could not authenticate with any available method"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP public key authentication success", "[PutSFTP][authentication]") {
  plan->setProperty(put, "Private Key Path", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"));
  plan->setProperty(put, "Private Key Passphrase", "privatekeypassword");

  createFile(src_dir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Successfully authenticated with publickey"));
  testFile("nifi_test/tstFile.ext", "tempFile");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP public key authentication bad passphrase", "[PutSFTP][authentication]") {
  plan->setProperty(put, "Password", "");
  plan->setProperty(put, "Private Key Path", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"));
  plan->setProperty(put, "Private Key Passphrase", "badpassword");

  createFile(src_dir, "tstFile.ext", "tempFile");

  try {
    testController.runSession(plan, true);
  } catch (std::exception &e) {
    std::string expected = minifi::Exception(minifi::PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow").what();
    REQUIRE(0 == std::string(e.what()).compare(0, expected.size(), expected));
  }
  REQUIRE(LogTestController::getInstance().contains(PUBLIC_KEY_AUTH_ERROR_MESSAGE));
  REQUIRE(LogTestController::getInstance().contains("Could not authenticate with any available method"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP public key authentication bad passphrase fallback to password", "[PutSFTP][authentication]") {
  plan->setProperty(put, "Private Key Path", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"));
  plan->setProperty(put, "Private Key Passphrase", "badpassword");

  createFile(src_dir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains(PUBLIC_KEY_AUTH_ERROR_MESSAGE));
  REQUIRE(LogTestController::getInstance().contains("Successfully authenticated with password"));
  testFile("nifi_test/tstFile.ext", "tempFile");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP host key checking success", "[PutSFTP][hostkey]") {
  plan->setProperty(put, "Host Key File", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/known_hosts"));
  plan->setProperty(put, "Strict Host Key Checking", "true");

  createFile(src_dir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Host key verification succeeded for localhost"));
  testFile("nifi_test/tstFile.ext", "tempFile");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP host key checking missing strict", "[PutSFTP][hostkey]") {
  plan->setProperty(put, "Hostname", "127.0.0.1");

  plan->setProperty(put, "Host Key File", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/known_hosts"));
  plan->setProperty(put, "Strict Host Key Checking", "true");

  createFile(src_dir, "tstFile.ext", "tempFile");

  try {
    testController.runSession(plan, true);
  } catch (std::exception &e) {
    std::string expected = minifi::Exception(minifi::PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow").what();
    REQUIRE(0 == std::string(e.what()).compare(0, expected.size(), expected));
  }

  REQUIRE(LogTestController::getInstance().contains("Host 127.0.0.1 not found in the host key file"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP host key checking missing non-strict", "[PutSFTP][hostkey]") {
  plan->setProperty(put, "Hostname", "127.0.0.1");

  plan->setProperty(put, "Host Key File", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/known_hosts"));
  plan->setProperty(put, "Strict Host Key Checking", "false");

  createFile(src_dir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Host 127.0.0.1 not found in the host key file"));
  testFile("nifi_test/tstFile.ext", "tempFile");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP host key checking mismatch strict", "[PutSFTP][hostkey]") {
  plan->setProperty(put, "Host Key File", utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/known_hosts_mismatch"));
  plan->setProperty(put, "Strict Host Key Checking", "true");

  createFile(src_dir, "tstFile.ext", "tempFile");

  try {
    testController.runSession(plan, true);
  } catch (std::exception &e) {
    std::string expected = minifi::Exception(minifi::PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow").what();
    REQUIRE(0 == std::string(e.what()).compare(0, expected.size(), expected));
  }

  REQUIRE(LogTestController::getInstance().contains("Host key mismatch for localhost, expected: "
                                                    "AAAAB3NzaC1yc2EAAAADAQABAAABAQCueV6fHbTECMPps4nXJ9jiVcxArTKXYip+"
                                                    "SEIBwkvmQiEDj4/zldU4KUn4QRwqbFmR9JO3s3SkPVzvP9bKh2Xk3nICB73iMs4v"
                                                    "wO2nZKpkBFtNz6+w0LsqDzQe9piW0ukoXw2Ce41yQK+9xtugPVHbbchP0esDanDf"
                                                    "SGjbQyPsmarfuJ8K+ACgGmWB9GKSthq7j+gArgefz0SGHkoKXA+3OXF/D6/MnLLv"
                                                    "H1sYmVyIO6CzDiurHUS2o7MWjiw3qK+n6o9rW0fpLFM/l3+8dLCt3e6D+lLQsJVX"
                                                    "iL1TJVu+Lf2z9kMc+3brFjFNVBRDxjYMVjOhUr6JyU3ouL1i6P/9"
                                                    ", actual: "
                                                    "AAAAB3NzaC1yc2EAAAADAQABAAABAQCrRDRfH278iGChp1a5hSMzQcDd63YoA2Np"
                                                    "VELEXzKmrPOuUgQXpdzdxk17oTdh5D+xTax2sTa3ZR55RWl1q6keqnrRvihWpBqF"
                                                    "N6D0aKUmGe/9Xlxhfe8v/RBs3j9JxlmbPtIFwKXF1ePfLFaI6n1BbXs0WUR8M7Cw"
                                                    "4OMFYTuvQ7IcrPE/FU+Xh4hdNm6y3j0ppKBj3LnZOABI/Ql/fyJUTpRqrLIqqdMi"
                                                    "3zxLNVBx7mVZQZICO1IPkh7ZqT0s3HyGT5hlsVuZsRfpqOjT4QfBeNjWuHdB5cDs"
                                                    "mDbaOvO6iQ/NY63uy7t/2VGmSASSzi4wzILvXKQTL3Lx5de9Tol7"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution rename", "[PutSFTP][conflict-resolution]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/1.tstFile1.ext", "content 1");
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution reject", "[PutSFTP][conflict-resolution]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REJECT);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship reject"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution fail", "[PutSFTP][conflict-resolution]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_FAIL);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution ignore", "[PutSFTP][conflict-resolution]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_IGNORE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Routing tstFile1.ext to SUCCESS despite a file with the same name already existing"));
  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution replace", "[PutSFTP][conflict-resolution]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REPLACE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/tstFile1.ext", "content 1");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution none", "[PutSFTP][conflict-resolution]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_NONE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution with directory existing at target", "[PutSFTP][conflict-resolution]") {
  bool should_predetect_failure = true;
  SECTION("with conflict resolution rename") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);
  }
  SECTION("with conflict resolution reject") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REJECT);
  }
  SECTION("with conflict resolution fail") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_FAIL);
  }
  SECTION("with conflict resolution ignore") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_IGNORE);
  }
  SECTION("with conflict resolution replace") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REPLACE);
  }
  SECTION("with conflict resolution none") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_NONE);
    should_predetect_failure = false;
  }

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test/tstFile1.ext")));

  testController.runSession(plan, true);

  if (should_predetect_failure) {
    REQUIRE(LogTestController::getInstance().contains("Rejecting tstFile1.ext because a directory with the same name already exists"));
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship reject"));
  } else {
    REQUIRE(LogTestController::getInstance().contains("Failed to rename remote file \"nifi_test/.tstFile1.ext\" to \"nifi_test/tstFile1.ext\", error: LIBSSH2_FX_FILE_ALREADY_EXISTS"));
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  }
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP reject zero-byte false", "[PutSFTP]") {
  plan->setProperty(put, "Reject Zero-Byte Files", "false");

  createFile(src_dir, "tstFile1.ext", "");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/tstFile1.ext", "");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP reject zero-byte true", "[PutSFTP]") {
  plan->setProperty(put, "Reject Zero-Byte Files", "true");

  createFile(src_dir, "tstFile1.ext", "");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Rejecting tstFile1.ext because it is zero bytes"));
  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship reject"));
  testFileNotExists("nifi_test/tstFile1.ext");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP set mtime", "[PutSFTP]") {
  plan->setProperty(put, "Last Modified Time", "2065-01-24T05:20:00Z");

  createFile(src_dir, "tstFile1.ext", "content 1");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile1.ext", "content 1");
  testModificationTime("nifi_test/tstFile1.ext", 3000000000LL);
}

#ifndef WIN32
TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP set permissions", "[PutSFTP]") {
  plan->setProperty(put, "Permissions", "0613");

  createFile(src_dir, "tstFile1.ext", "content 1");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile1.ext", "content 1");
  testPermissions("nifi_test/tstFile1.ext", 0613);
}
#endif

#ifndef WIN32
TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP set uid and gid", "[PutSFTP]") {
  if (getuid() != 0) {
    std::cerr << "!!!! This test ONLY works as root, because it needs to chown. Exiting. !!!!" << std::endl;
    return;
  }
  plan->setProperty(put, "Remote Owner", "1234");
  plan->setProperty(put, "Remote Group", "4567");

  createFile(src_dir, "tstFile1.ext", "content 1");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile1.ext", "content 1");
  testOwner("nifi_test/tstFile1.ext", 1234);
  testGroup("nifi_test/tstFile1.ext", 4567);
}
#endif

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP disable directory creation", "[PutSFTP]") {
  plan->setProperty(put, "Create Directory", "false");

  createFile(src_dir, "tstFile1.ext", "content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  testFileNotExists("nifi_test/tstFile1.ext");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP test dot rename", "[PutSFTP]") {
  bool should_fail = false;
  SECTION("with dot rename enabled") {
    plan->setProperty(put, "Dot Rename", "true");
    should_fail = true;
  }
  SECTION("with dot rename disabled") {
    plan->setProperty(put, "Dot Rename", "false");
    should_fail = false;
  }

  createFile(src_dir, "tstFile1.ext", "content 1");

  /*
   * We create the would-be dot renamed file in the target, and because we don't overwrite temporary files,
   * if we really use a dot renamed temporary file, we should fail.
   */
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/.tstFile1.ext", "");

  testController.runSession(plan, true);

  if (should_fail) {
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
    testFileNotExists("nifi_test/tstFile1.ext");
  } else {
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
    testFile("nifi_test/tstFile1.ext", "content 1");
  }
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP test temporary filename", "[PutSFTP]") {
  bool should_fail = false;
  SECTION("with temporary filename set") {
    /* Also test expression language */
    plan->setProperty(put, "Temporary Filename", "${ filename:append('.temp') }");
    should_fail = true;
  }
  SECTION("with temporary filename not set and dot rename disabled") {
    plan->setProperty(put, "Dot Rename", "false");
    should_fail = false;
  }

  createFile(src_dir, "tstFile1.ext", "content 1");

  /*
   * We create the would-be temporary file in the target, and because we don't overwrite temporary files,
   * if we really use the temporary file, we should fail.
   */
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext.temp", "");

  testController.runSession(plan, true);

  if (should_fail) {
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
    testFileNotExists("nifi_test/tstFile1.ext");
  } else {
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
    testFile("nifi_test/tstFile1.ext", "content 1");
  }
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP test temporary file cleanup", "[PutSFTP]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_NONE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  testFile("nifi_test/tstFile1.ext", "content 2");
  testFileNotExists("nifi_test/.tstFile1.ext");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP test disable directory listing", "[PutSFTP]") {
  bool should_list = false;
  SECTION("with directory listing enabled") {
    plan->setProperty(put, "Disable Directory Listing", "false");
    should_list = true;
  }
  SECTION("with directory listing disabled") {
    plan->setProperty(put, "Disable Directory Listing", "true");
    should_list = false;
  }

  createFile(src_dir, "tstFile1.ext", "content 1");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFileNotExists("nifi_test/inner/tstFile1.ext");

  REQUIRE(should_list == LogTestController::getInstance().contains("Failed to stat remote path \"nifi_test\", error: LIBSSH2_FX_NO_SUCH_FILE"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP connection caching reuse", "[PutSFTP][connection-caching]") {
  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile1.ext", "content 1");
  testFile("nifi_test/tstFile2.ext", "content 2");

  REQUIRE(LogTestController::getInstance().contains("Adding nifiuser@localhost:" + std::to_string(sftp_server->getPort()) + " to SFTP connection pool"));
  REQUIRE(LogTestController::getInstance().contains("Removing nifiuser@localhost:" + std::to_string(sftp_server->getPort()) + " from SFTP connection pool"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP connection caching does not reuse bad connection", "[PutSFTP][connection-caching]") {
  createFile(src_dir, "tstFile1.ext", "content 1");

  /* Simulate connection failure */
  auto port = sftp_server->getPort();
  sftp_server.reset();

  try {
    testController.runSession(plan, true);
  } catch (std::exception &e) {
    std::string expected = minifi::Exception(minifi::PROCESS_SESSION_EXCEPTION, "Can not find the transfer relationship for the updated flow").what();
    REQUIRE(0 == std::string(e.what()).compare(0, expected.size(), expected));
  }

  testController.runSession(plan, true);

  REQUIRE(false == LogTestController::getInstance().contains("Adding nifiuser@localhost:" + std::to_string(port) + " to SFTP connection pool"));
  REQUIRE(LogTestController::getInstance().contains("Cannot connect to SFTP server"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP connection caching reaches limit", "[PutSFTP][connection-caching]") {
  plan = testController.createPlan();
  get_file = plan->addProcessor(
      "GetFile",
      "GetFile");
  auto extract_text = plan->addProcessor(
      "ExtractText",
      "ExtractText",
      core::Relationship("success", "d"),
      true);
  put = plan->addProcessor(
      "PutSFTP",
      "PutSFTP",
      core::Relationship("success", "d"),
      true);
  plan->addProcessor("LogAttribute",
      "LogAttribute",
      { core::Relationship("success", "d"),
      core::Relationship("reject", "d"),
      core::Relationship("failure", "d") },
      true);

  // Configure GetFile processor
  plan->setProperty(get_file, "Batch Size", "1");
  plan->setProperty(get_file, "Input Directory", src_dir);

  // Configure ExtractText processor
  plan->setProperty(extract_text, "Attribute", "port_num");

  // Configure PutSFTP processor
  plan->setProperty(put, "Hostname", "localhost");
  plan->setProperty(put, "Port", "${'port_num'}");
  plan->setProperty(put, "Username", "nifiuser");
  plan->setProperty(put, "Password", "nifipassword");
  plan->setProperty(put, "Remote Path", "nifi_test/");
  plan->setProperty(put, "Create Directory", "true");
  plan->setProperty(put, "Batch Size", "2");
  plan->setProperty(put, "Connection Timeout", "30 sec");
  plan->setProperty(put, "Data Timeout", "30 sec");
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);
  plan->setProperty(put, "Strict Host Key Checking", "false");
  plan->setProperty(put, "Send Keep Alive On Timeout", "true");
  plan->setProperty(put, "Use Compression", "false");
  plan->setProperty(put, "Reject Zero-Byte Files", "true");

  sftp_server.reset();

  std::vector<std::unique_ptr<SFTPTestServer>> sftp_servers;

  for (size_t i = 0; i < 10; i++) {
    std::string destination_dir = testController.createTempDirectory();
    sftp_servers.emplace_back(new SFTPTestServer(destination_dir));
    REQUIRE(true == sftp_servers.back()->start());
    createFile(src_dir, "tstFile" + std::to_string(i) + ".ext", std::to_string(sftp_servers.back()->getPort()));

    testController.runSession(plan, true);
    plan->reset();

    if (i == 8) {
      REQUIRE(LogTestController::getInstance().contains("SFTP connection pool is full, removing nifiuser@localhost:" + std::to_string(sftp_servers[0]->getPort())));
      REQUIRE(LogTestController::getInstance().contains("Closing SFTPClient for localhost:" + std::to_string(sftp_servers[0]->getPort())));
      REQUIRE(LogTestController::getInstance().contains("Adding nifiuser@localhost:" + std::to_string(sftp_servers[8]->getPort()) + " to SFTP connection pool"));
    } else if (i == 9) {
      REQUIRE(LogTestController::getInstance().contains("SFTP connection pool is full, removing nifiuser@localhost:" + std::to_string(sftp_servers[1]->getPort())));
      REQUIRE(LogTestController::getInstance().contains("Closing SFTPClient for localhost:" + std::to_string(sftp_servers[1]->getPort())));
      REQUIRE(LogTestController::getInstance().contains("Adding nifiuser@localhost:" + std::to_string(sftp_servers[9]->getPort()) + " to SFTP connection pool"));
    }
  }
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP batching two files in one batch", "[PutSFTP][batching]") {
  plan->setProperty(put, "Batch Size", "2");

  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(2U == directoryContentCount(std::string(dst_dir) + "/vfs/nifi_test"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP batching two files in two batches", "[PutSFTP][batching]") {
  plan->setProperty(put, "Batch Size", "1");

  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");

  testController.runSession(plan, true);
  REQUIRE(1U == directoryContentCount(std::string(dst_dir) + "/vfs/nifi_test"));
  plan->reset();

  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");

  testController.runSession(plan, true);
  REQUIRE(2U == directoryContentCount(std::string(dst_dir) + "/vfs/nifi_test"));
  plan->reset();
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP batching does not fail even if one is routed to Failure", "[PutSFTP][batching]") {
  plan->setProperty(put, "Batch Size", "3");
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_FAIL);

  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");
  createFile(src_dir, "tstFile3.ext", "content 3");

  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content other");
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile2.ext", "content other");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile3.ext", "content 3");

  REQUIRE(LogTestController::getInstance().contains("Routing tstFile1.ext to FAILURE because a file with the same name already exists"));
  REQUIRE(LogTestController::getInstance().contains("Routing tstFile2.ext to FAILURE because a file with the same name already exists"));
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP put large file", "[PutSFTP]") {
  std::mt19937 rng(std::random_device{}()); // NOLINT
  std::string content(4 * 1024 * 1024U, '\0');
  std::generate_n(content.begin(), 4 * 1024 * 1024U, std::ref(rng));

  createFile(src_dir, "tstFile.ext", content);

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile.ext", content);
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP expression language test", "[PutSFTP]") {
  plan = testController.createPlan();
  get_file = plan->addProcessor(
      "GetFile",
      "GetFile");
  auto update_attribute = plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "d"),
      true);
  put = plan->addProcessor(
      "PutSFTP",
      "PutSFTP",
      core::Relationship("success", "d"),
      true);
  plan->addProcessor("LogAttribute",
      "LogAttribute",
      { core::Relationship("success", "d"),
      core::Relationship("reject", "d"),
      core::Relationship("failure", "d") },
      true);

  // Configure GetFile processor
  plan->setProperty(get_file, "Input Directory", src_dir);

  // Configure UpdateAttribute processor
  plan->setProperty(update_attribute, "attr_Hostname", "localhost", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Port", std::to_string(sftp_server->getPort()), true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Username", "nifiuser", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Password", "nifipassword", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Private Key Path",
      utils::file::FileUtils::concat_path(get_sftp_test_dir(), "resources/id_rsa"), true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Private Key Passphrase", "privatekeypassword", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Remote Path", "nifi_test/", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Temporary Filename", "tempfile", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Last Modified Time", "2065-01-24T05:20:00Z", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Permissions", "rw-------", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Remote Owner", "1234", true /*dynamic*/);
  plan->setProperty(update_attribute, "attr_Remote Group", "5678", true /*dynamic*/);

  // Configure PutSFTP processor
  plan->setProperty(put, "Hostname", "${'attr_Hostname'}");
  plan->setProperty(put, "Port", "${'attr_Port'}");
  plan->setProperty(put, "Username", "${'attr_Username'}");
  plan->setProperty(put, "Password", "${'attr_Password'}");
  plan->setProperty(put, "Private Key Path", "${'attr_Private Key Path'}");
  plan->setProperty(put, "Private Key Passphrase", "${'attr_Private Key Passphrase'}");
  plan->setProperty(put, "Remote Path", "${'attr_Remote Path'}");
  plan->setProperty(put, "Temporary Filename", "${'attr_Temporary Filename'}");
  plan->setProperty(put, "Last Modified Time", "${'attr_Last Modified Time'}");
  plan->setProperty(put, "Permissions", "${'attr_Permissions'}");
  plan->setProperty(put, "Remote Owner", "${'attr_Remote Owner'}");
  plan->setProperty(put, "Remote Group", "${'attr_Remote Group'}");
  plan->setProperty(put, "Create Directory", "true");
  plan->setProperty(put, "Batch Size", "2");
  plan->setProperty(put, "Connection Timeout", "30 sec");
  plan->setProperty(put, "Data Timeout", "30 sec");
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);
  plan->setProperty(put, "Strict Host Key Checking", "false");
  plan->setProperty(put, "Send Keep Alive On Timeout", "true");
  plan->setProperty(put, "Use Compression", "false");
  plan->setProperty(put, "Reject Zero-Byte Files", "true");

  createFile(src_dir, "tstFile.ext", "some content");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile.ext", "some content");
}
