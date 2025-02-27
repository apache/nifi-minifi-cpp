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
#include <cstdio>
#include <string>
#include <iostream>
#include "processors/InvokeHTTP.h"
#include "processors/LogAttribute.h"
#include "unit/TestBase.h"
#include "core/logging/Logger.h"
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
  HttpTestHarness() {
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
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
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
  HttpTestHarness harness;
  harness.setKeyDir(TEST_RESOURCES);
  SECTION("Without chunked encoding") {
    const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPPost.yml";
    harness.run(test_file_path);
  }
#ifndef __APPLE__
  SECTION("With chunked encoding") {
    const auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestHTTPPostChunkedEncoding.yml";
    harness.run(test_file_path);
  }
#endif
}

}  // namespace org::apache::nifi::minifi::test
