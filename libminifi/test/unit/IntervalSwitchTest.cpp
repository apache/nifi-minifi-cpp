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

#include "../TestBase.h"
#include "../Catch.h"
#include "utils/IntervalSwitch.h"

namespace utils = org::apache::nifi::minifi::utils;

namespace {
using State = utils::IntervalSwitchState;

struct expected {
  expected(State state, bool switched) :state{state}, switched{switched} {}

  State state;
  bool switched;
};

template<typename IntervalSwitchReturn>
bool operator==(const IntervalSwitchReturn& lhs, const expected& c) {
  return lhs.state == c.state && lhs.switched == c.switched;
}
}  // namespace

TEST_CASE("basic IntervalSwitch", "[intervalswitch.basic]") {
  utils::IntervalSwitch<int> interval_switch{100, 150};
  REQUIRE(interval_switch(120) == expected(State::UPPER, false));
  REQUIRE(interval_switch(100) == expected(State::UPPER, false));
  REQUIRE(interval_switch(99) == expected(State::LOWER, true));
  REQUIRE(interval_switch(-20022202) == expected(State::LOWER, false));
  REQUIRE(interval_switch(120) == expected(State::LOWER, false));
  REQUIRE(interval_switch(149) == expected(State::LOWER, false));
  REQUIRE(interval_switch(150) == expected(State::UPPER, true));
  REQUIRE(interval_switch(150) == expected(State::UPPER, false));
  REQUIRE(interval_switch(2000) == expected(State::UPPER, false));
  REQUIRE(interval_switch(120) == expected(State::UPPER, false));
  REQUIRE(interval_switch(100) == expected(State::UPPER, false));
}

TEST_CASE("IntervalSwitch comparator", "[intervalswitch.comp]") {
  utils::IntervalSwitch<uint32_t, std::greater<>> interval_switch{250, 100};
  REQUIRE(interval_switch(99) == expected(State::UPPER, false));
  REQUIRE(interval_switch(120) == expected(State::UPPER, false));
  REQUIRE(interval_switch(250) == expected(State::UPPER, false));
  REQUIRE(interval_switch(251) == expected(State::LOWER, true));
  REQUIRE(interval_switch(150) == expected(State::LOWER, false));
  REQUIRE(interval_switch(101) == expected(State::LOWER, false));
  REQUIRE(interval_switch(100) == expected(State::UPPER, true));
  REQUIRE(interval_switch(100) == expected(State::UPPER, false));
  REQUIRE(interval_switch(250) == expected(State::UPPER, false));
}
