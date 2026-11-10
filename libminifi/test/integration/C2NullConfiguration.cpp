// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <memory>
#include <string>
#include <iostream>
#include "processors/InvokeHTTP.h"
#include "unit/TestBase.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "integration/TestServer.h"
#include "c2/C2Agent.h"
#include "processors/LogAttribute.h"
#include "integration/HTTPIntegrationBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class VerifyC2Server : public HTTPIntegrationBase {
 public:
  explicit VerifyC2Server(const std::filesystem::path& test_file_path)
      : HTTPIntegrationBase(test_file_path) {
    dir = testController.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<c2::C2Agent>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<core::ProcessSession>();
    std::fstream file;
    auto path = dir / "tstFile.ext";
    file.open(path, std::ios::out);
    file << "tempFile";
    file.close();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_), "C2Agent] [error] Could not instantiate null"));
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> pg) override {
    auto* const proc = pg->findProcessorByName("invoke");
    REQUIRE(proc != nullptr);

    proc->getImpl<minifi::processors::InvokeHTTP>();
    std::string url = proc->getProperty(processors::InvokeHTTP::URL.name).value_or("");

    std::string port;
    std::string scheme;
    std::string path;
    minifi::utils::parse_http_components(url, port, scheme, path);
    configuration->set(Configuration::nifi_c2_enable, "true");
    configuration->set(Configuration::nifi_c2_agent_class, "test");
    configuration->set(Configuration::nifi_c2_rest_url, "");
    configuration->set(Configuration::nifi_c2_rest_url_ack, "");
    configuration->set(Configuration::nifi_c2_agent_heartbeat_reporter_classes, "null");
    configuration->set(Configuration::nifi_c2_rest_listener_port, "null");
    configuration->set(Configuration::nifi_c2_agent_heartbeat_period, "null");
  }

 protected:
  std::filesystem::path dir;
  TestController testController;
};

TEST_CASE("C2NullConfiguration", "[c2test]") {
  VerifyC2Server harness(std::filesystem::path(TEST_RESOURCES) / "TestNull.yml");
  harness.setKeyDir(TEST_RESOURCES);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
