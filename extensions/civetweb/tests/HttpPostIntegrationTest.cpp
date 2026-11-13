// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <cstdio>
#include <string>
#include <iostream>
#include "processors/InvokeHTTP.h"
#include "processors/LogAttribute.h"
#include "unit/TestBase.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "FlowController.h"
#include "integration/HTTPIntegrationBase.h"
#include "unit/TestUtils.h"
#include "properties/Configuration.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class HttpTestHarness : public HTTPIntegrationBase {
 public:
  explicit HttpTestHarness(const std::filesystem::path& test_file_location) : HTTPIntegrationBase(test_file_location) {
    dir_ = test_controller_.createTempDirectory();
  }

  void testSetup() override {
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<core::ProcessContext>();
    LogTestController::getInstance().setTrace<minifi::processors::InvokeHTTP>();
    LogTestController::getInstance().setDebug<minifi::http::HTTPClient>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setDebug<core::Processor>();
    LogTestController::getInstance().setDebug<minifi::ThreadedSchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::TimerDrivenSchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
    std::fstream file;
    test_file_ = dir_ / "tstFile.ext";
    file.open(test_file_, std::ios::out);
    file << "tempFile";
    file.close();
    configuration->set(minifi::Configuration::nifi_flow_engine_threads, "8");
    configuration->set(minifi::Configuration::nifi_c2_enable, "false");
  }

  void cleanup() override {
    std::filesystem::remove(test_file_);
    IntegrationBase::cleanup();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(wait_time_,
      "curl performed",
      "Size:1024 Offset:0"));
    REQUIRE_FALSE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(200), "Size:0 Offset:0"));
  }

 protected:
  std::filesystem::path dir_;
  std::filesystem::path test_file_;
  TestController test_controller_;
};

TEST_CASE("Test HTTP client POST request", "[httptest]") {
  std::filesystem::path test_file_path;
  SECTION("Without chunked encoding") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPPost.yml";
  }
#ifndef __APPLE__
  SECTION("With chunked encoding") {
    test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPPostChunkedEncoding.yml";
  }
#endif
  HttpTestHarness harness(test_file_path);
  harness.setKeyDir(TEST_RESOURCES);
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
