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
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <utility>
#include <string>
#include <memory>
#include <ctime>
#include "../TestBase.h"

TEST_CASE("Test log Levels", "[ttl1]") {
  LogTestController::getInstance().setLevel<logging::Logger>(spdlog::level::trace);
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_info("hello %s", "world");

  REQUIRE(
      true
          == LogTestController::getInstance().contains(
              "[org::apache::nifi::minifi::core::logging::Logger] [info] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels debug", "[ttl2]") {
  LogTestController::getInstance().setLevel<logging::Logger>(spdlog::level::trace);
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_debug("hello %s", "world");

  REQUIRE(
      true
          == LogTestController::getInstance().contains(
              "[org::apache::nifi::minifi::core::logging::Logger] [debug] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels trace", "[ttl3]") {
  LogTestController::getInstance().setLevel<logging::Logger>(spdlog::level::trace);
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_trace("hello %s", "world");

  REQUIRE(
      true
          == LogTestController::getInstance().contains(
              "[org::apache::nifi::minifi::core::logging::Logger] [trace] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels error", "[ttl4]") {
  LogTestController::getInstance().setLevel<logging::Logger>(spdlog::level::trace);
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_error("hello %s", "world");

  REQUIRE(
      true
          == LogTestController::getInstance().contains(
              "[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels change", "[ttl5]") {
  LogTestController::getInstance().setLevel<logging::Logger>(spdlog::level::trace);
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_error("hello %s", "world");

  REQUIRE(
      true
          == LogTestController::getInstance().contains(
              "[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setLevel<logging::Logger>(spdlog::level::off);
  logger->log_error("hello %s", "world");

  REQUIRE(
      false
          == LogTestController::getInstance().contains(
              "[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
}
