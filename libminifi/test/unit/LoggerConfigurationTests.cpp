// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "core/logging/LoggerConfiguration.h"
#include "spdlog/formatter.h"
#include "spdlog/pattern_formatter.h"
#include "spdlog/sinks/ostream_sink.h"

TEST_CASE("TestLoggerProperties::get_keys_of_type", "[test get_keys_of_type]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<logging::LoggerProperties>();
  logging::LoggerProperties logger_properties{""};
  logger_properties.set("appender.rolling", "rolling");
  logger_properties.set("notappender.notrolling", "notrolling");
  logger_properties.set("appender.stdout", "stdout");
  logger_properties.set("appender.stdout2.ignore", "stdout");
  std::vector<std::string> expected = { "appender.rolling", "appender.stdout" };
  std::vector<std::string> actual = logger_properties.get_keys_of_type("appender");
  std::sort(actual.begin(), actual.end());
  REQUIRE(expected == actual);
}
