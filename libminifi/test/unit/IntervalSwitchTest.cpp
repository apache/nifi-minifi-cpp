// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "unit/TestBase.h"
#include "unit/Catch.h"
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
