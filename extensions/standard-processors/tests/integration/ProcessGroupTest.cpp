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
  ProcessGroupTestHarness() : IntegrationBase(2s) {
  }

  void testSetup() override {
    LogTestController::getInstance().setInfo<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::processors::UpdateAttribute>();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::milliseconds(wait_time_),
        "key:test_attribute value:success"));
  }
};

TEST_CASE("ProcessGroupTest", "[ProcessGroupTest]") {
  ProcessGroupTestHarness harness;
  auto test_file_path = std::filesystem::path(TEST_RESOURCES) / "TestProcessGroup.yml";
  harness.run(test_file_path);
}

}  // namespace org::apache::nifi::minifi::test
