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

#undef NDEBUG

#include "TestBase.h"
#include "Catch.h"

#include "c2/C2Agent.h"
#include "c2/HeartbeatLogger.h"
#include "protocols/RESTProtocol.h"
#include "protocols/RESTSender.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "range/v3/action/sort.hpp"
#include "range/v3/action/unique.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/filter.hpp"
#include "range/v3/view/split.hpp"
#include "range/v3/view/transform.hpp"
#include "utils/IntegrationTestUtils.h"
#include "utils/StringUtils.h"
#include "properties/Configuration.h"

class VerifyLogC2Heartbeat : public VerifyC2Base {
 public:
  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::c2::C2Agent>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTSender>();
    LogTestController::getInstance().setDebug<minifi::c2::RESTProtocol>();
    // the heartbeat is logged at TRACE level
    LogTestController::getInstance().setTrace<minifi::c2::HeartbeatLogger>();
    VerifyC2Base::testSetup();
  }

  void runAssertions() override {
    using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
    assert(verifyLogLinePresenceInPollTime(
        std::chrono::milliseconds(wait_time_),
        "\"operation\": \"heartbeat\""));

    const auto log = LogTestController::getInstance().getLogs();
    auto types_in_heartbeat = log | ranges::views::split('\n')
        | ranges::views::transform([](auto&& rng) { return rng | ranges::to<std::string>; })
        | ranges::views::filter([](auto&& line) { return utils::StringUtils::startsWith(line, "                                \"type\":"); })
        | ranges::to<std::vector<std::string>>;
    const auto num_types = types_in_heartbeat.size();
    types_in_heartbeat |= ranges::actions::sort | ranges::actions::unique;
    const auto num_distinct_types = types_in_heartbeat.size();
    assert(num_types == num_distinct_types);
  }

  void configureC2() override {
    VerifyC2Base::configureC2();
    configuration->set(org::apache::nifi::minifi::Configuration::nifi_c2_agent_heartbeat_reporter_classes, "HeartbeatLogger");
  }
};

int main() {
  VerifyLogC2Heartbeat harness;
  HeartbeatHandler responder(harness.getConfiguration());
  harness.setUrl("https://localhost:0/heartbeat", &responder);
  harness.run();
}
