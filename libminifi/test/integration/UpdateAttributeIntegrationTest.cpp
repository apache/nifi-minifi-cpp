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
