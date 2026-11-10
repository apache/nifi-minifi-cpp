// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include "processors/InvokeHTTP.h"
#include "unit/TestBase.h"
#include "core/ProcessGroup.h"
#include "properties/Configure.h"
#include "integration/TestServer.h"
#include "integration/HTTPIntegrationBase.h"
#include "unit/TestUtils.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class VerifyC2Server : public HTTPIntegrationBase {
 public:
  explicit VerifyC2Server(const std::filesystem::path& test_file_location) : HTTPIntegrationBase(test_file_location) {
    dir_ = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
    std::fstream file;
    test_file_ = dir_ / "tstFile.ext";
    file.open(test_file_, std::ios::out);
    file << "tempFile";
    file.close();
  }

  void cleanup() override {
    std::filesystem::remove(test_file_);
    IntegrationBase::cleanup();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(wait_time_,
        "Import offset 0",
        "Outputting success and response"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    auto proc = pg->findProcessorByName("invoke");
    REQUIRE(proc != nullptr);

    proc->getImpl<minifi::processors::InvokeHTTP>();
    std::string url = proc->getProperty(processors::InvokeHTTP::URL.name).value_or("");

    std::string port;
    std::string scheme;
    std::string path;
    minifi::utils::parse_http_components(url, port, scheme, path);
    configuration->set(minifi::Configuration::nifi_c2_enable, "true");
    configuration->set(minifi::Configuration::nifi_c2_agent_class, "test");
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_reporter_classes, "RESTReceiver");
    configuration->set(minifi::Configuration::nifi_c2_rest_listener_port, port);
    configuration->set(minifi::Configuration::nifi_c2_agent_heartbeat_period, "10");
  }

 protected:
  std::filesystem::path dir_;
  std::filesystem::path test_file_;
  TestController testController;
};

TEST_CASE("C2VerifyServeResults", "[c2test]") {
  VerifyC2Server harness(std::filesystem::path(TEST_RESOURCES) / "C2VerifyServeResults.yml");
  harness.setKeyDir(TEST_RESOURCES);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
