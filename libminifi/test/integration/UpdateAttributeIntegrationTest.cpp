// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <sys/stat.h>
#include <chrono>
#include <memory>
#include <string>
#include "core/ProcessContextImpl.h"
#include "processors/LogAttribute.h"
#include "integration/IntegrationBase.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"

namespace org::apache::nifi::minifi::test {

class TestHarness : public IntegrationBase {
 public:
  using IntegrationBase::IntegrationBase;

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::FlowController>();
    LogTestController::getInstance().setTrace<core::ProcessSession>();
    LogTestController::getInstance().setTrace<core::ProcessContextImpl>();
    LogTestController::getInstance().setInfo<minifi::processors::LogAttribute>();
  }

  void runAssertions() override {
    REQUIRE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "key:route_check_attr value:good",
        "key:variable_attribute value:replacement_value"));
    REQUIRE_FALSE(minifi::test::utils::verifyLogLinePresenceInPollTime(std::chrono::milliseconds(200), "ProcessSession rollback"));  // No rollback happened
  }

  void queryRootProcessGroup(std::shared_ptr<core::ProcessGroup> /*pg*/) override {
    // inject the variable into the context.
    configuration->set("nifi.variable.test", "replacement_value");
  }
};

TEST_CASE("UpdateAttributeIntegrationTest", "[updateattribute]") {
  TestHarness harness(std::filesystem::path(TEST_RESOURCES) / "TestUpdateAttribute.yml");
  harness.run();
}

}  // namespace org::apache::nifi::minifi::test
