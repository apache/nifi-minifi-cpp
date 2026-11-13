// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <string>

#include "minifi-cpp/core/logging/Logger.h"
#include "FlowController.h"
#include "unit/TestBase.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "integration/IntegrationBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::test {

class ProcessGroupTestHarness : public IntegrationBase {
 public:
  explicit ProcessGroupTestHarness(const std::filesystem::path& test_file_location) : IntegrationBase(test_file_location, {}, 2s) {
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::processors::UpdateAttribute>();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
    REQUIRE(verifyLogLinePresenceInPollTime(wait_time_, "key:test_attribute value:success"));
  }
};

TEST_CASE("ProcessGroupTest", "[ProcessGroupTest]") {
  ProcessGroupTestHarness harness(std::filesystem::path(TEST_RESOURCES) / "TestProcessGroup.yml");
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
