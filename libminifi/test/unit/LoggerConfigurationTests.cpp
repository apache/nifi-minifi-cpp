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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "../TestBase.h"
#include "core/logging/LoggerConfiguration.h"
#include "spdlog/formatter.h"

TEST_CASE("TestLoggerProperties::get_keys_of_type", "[test get_keys_of_type]") {
  TestController test_controller;
  LogTestController::getInstance().setDebug<logging::LoggerProperties>();
  logging::LoggerProperties logger_properties;
  logger_properties.set("appender.rolling", "rolling");
  logger_properties.set("notappender.notrolling", "notrolling");
  logger_properties.set("appender.stdout", "stdout");
  logger_properties.set("appender.stdout2.ignore", "stdout");
  std::vector<std::string> expected = { "appender.rolling", "appender.stdout" };
  std::vector<std::string> actual = logger_properties.get_keys_of_type("appender");
  std::sort(actual.begin(), actual.end());
  REQUIRE(expected == actual);
}

class TestLoggerConfiguration : public logging::LoggerConfiguration {
 public:
  static std::shared_ptr<logging::internal::LoggerNamespace> initialize_namespaces(const std::shared_ptr<logging::LoggerProperties> &logger_properties) {
    return logging::LoggerConfiguration::initialize_namespaces(logger_properties);
  }
  static std::shared_ptr<spdlog::logger> get_logger(const std::shared_ptr<logging::internal::LoggerNamespace> &root_namespace, const std::string &name, std::shared_ptr<spdlog::formatter> formatter) {
    return logging::LoggerConfiguration::get_logger(LogTestController::getInstance().logger_, root_namespace, name, formatter);
  }
};

TEST_CASE("TestLoggerConfiguration::initialize_namespaces", "[test initialize_namespaces]") {
  TestController test_controller;
  LogTestController &logTestController = LogTestController::getInstance();
  LogTestController::getInstance().setDebug<logging::LoggerProperties>();
  std::shared_ptr<logging::LoggerProperties> logger_properties = std::make_shared<logging::LoggerProperties>();

  std::ostringstream stdout;
  std::ostringstream stderr;
  logger_properties->add_sink("stdout", std::make_shared<spdlog::sinks::ostream_sink_mt>(stdout, true));
  logger_properties->add_sink("stderr", std::make_shared<spdlog::sinks::ostream_sink_mt>(stderr, true));

  std::string stdout_only_warn_class = "org::apache::nifi::minifi::fake::test::StdoutOnlyWarn";
  std::string stderr_only_error_pkg = "org::apache::nifi::minifi::fake2";
  std::string stderr_only_error_class = stderr_only_error_pkg + "::test::StderrOnlyError";
  logger_properties->set("logger.root", "INFO,stdout,stderr");
  logger_properties->set("logger." + stdout_only_warn_class, "WARN,stdout");
  logger_properties->set("logger." + stderr_only_error_pkg, "ERROR,stderr");

  std::shared_ptr<logging::internal::LoggerNamespace> root_namespace = TestLoggerConfiguration::initialize_namespaces(logger_properties);

  std::shared_ptr<spdlog::formatter> formatter = std::make_shared<spdlog::pattern_formatter>(logging::LoggerConfiguration::spdlog_default_pattern);
  std::shared_ptr<spdlog::logger> logger = TestLoggerConfiguration::get_logger(root_namespace, "org::apache::nifi::minifi::fake::test::ClassName1", formatter);
  std::string test_log_statement = "Test log statement";
  logger->info(test_log_statement);
  REQUIRE(true == logTestController.contains(stdout, test_log_statement));
  REQUIRE(true == logTestController.contains(stderr, test_log_statement));
  logTestController.resetStream(stdout);
  logTestController.resetStream(stderr);

  logger = TestLoggerConfiguration::get_logger(root_namespace, stdout_only_warn_class, formatter);
  logger->info(test_log_statement);
  REQUIRE(false == logTestController.contains(stdout, test_log_statement, std::chrono::seconds(0)));
  logger->warn(test_log_statement);
  REQUIRE(true == logTestController.contains(stdout, test_log_statement));
  REQUIRE(false == logTestController.contains(stderr, test_log_statement, std::chrono::seconds(0)));
  logTestController.resetStream(stdout);
  logTestController.resetStream(stderr);

  logger = TestLoggerConfiguration::get_logger(root_namespace, stderr_only_error_class, formatter);
  logger->warn(test_log_statement);
  REQUIRE(false == logTestController.contains(stderr, test_log_statement, std::chrono::seconds(0)));
  logger->error(test_log_statement);
  REQUIRE(false == logTestController.contains(stdout, test_log_statement, std::chrono::seconds(0)));
  REQUIRE(true == logTestController.contains(stderr, test_log_statement));
  logTestController.resetStream(stdout);
  logTestController.resetStream(stderr);
}
