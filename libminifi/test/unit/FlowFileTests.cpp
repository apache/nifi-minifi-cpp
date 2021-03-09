/**
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

#include "core/FlowFile.h"
#include "../TestBase.h"
#include "utils/TestUtils.h"

TEST_CASE("Repeated penalization doubles the penalty duration of FlowFiles", "[penalize]") {
  const auto TIMESTAMP = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  const auto PENALTY_DURATION = std::chrono::seconds{30};

  const auto clock = std::make_shared<utils::ManualClock>();
  clock->advance(TIMESTAMP);

  core::FlowFile flow_file{clock};

  SECTION("Initially, the penalty expiration is zero") {
    REQUIRE(flow_file.getPenaltyExpiration() == 0);
  }

  SECTION("When first penalized, a single penalty duration is added") {
    flow_file.penalize(PENALTY_DURATION);
    REQUIRE(std::chrono::milliseconds(flow_file.getPenaltyExpiration()) == TIMESTAMP + PENALTY_DURATION);
  }

  SECTION("For the second penalty, the penalty duration is doubled") {
    flow_file.penalize(PENALTY_DURATION);

    std::chrono::milliseconds TIME_ELAPSED{13000};
    clock->advance(TIME_ELAPSED);
    flow_file.penalize(PENALTY_DURATION);

    REQUIRE(std::chrono::milliseconds(flow_file.getPenaltyExpiration()) == TIMESTAMP + TIME_ELAPSED + 2 * PENALTY_DURATION);
  }

  SECTION("For the third penalty, the penalty duration is quadrupled") {
    flow_file.penalize(PENALTY_DURATION);

    std::chrono::milliseconds TIME_ELAPSED_1{77000};
    clock->advance(TIME_ELAPSED_1);
    flow_file.penalize(PENALTY_DURATION);

    std::chrono::milliseconds TIME_ELAPSED_2{45000};
    clock->advance(TIME_ELAPSED_2);
    flow_file.penalize(PENALTY_DURATION);

    REQUIRE(std::chrono::milliseconds(flow_file.getPenaltyExpiration()) == TIMESTAMP + TIME_ELAPSED_1 + TIME_ELAPSED_2 + 4 * PENALTY_DURATION);
  }

  SECTION("The maximum penalty duration is 1000 times the default duration") {
    for (int i = 0; i < 100; ++i) {
      flow_file.penalize(PENALTY_DURATION);
    }
    REQUIRE(std::chrono::milliseconds(flow_file.getPenaltyExpiration()) == TIMESTAMP + 1000 * PENALTY_DURATION);
  }

  SECTION("After the multiplier is reset, the duration of the next penalty is the same as the first one") {
    flow_file.penalize(PENALTY_DURATION);
    flow_file.penalize(PENALTY_DURATION);
    flow_file.penalize(PENALTY_DURATION);
    flow_file.resetPenaltyMultiplier();
    flow_file.penalize(PENALTY_DURATION);
    REQUIRE(std::chrono::milliseconds(flow_file.getPenaltyExpiration()) == TIMESTAMP + PENALTY_DURATION);
  }
}
