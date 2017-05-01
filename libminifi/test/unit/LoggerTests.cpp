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

#include <memory>
#include <ctime>
#include "../TestBase.h"
#include "core/logging/LogAppenders.h"

using namespace logging;

bool contains(std::string stringA, std::string ending) {
  return (ending.length() > 0 && stringA.find(ending) != std::string::npos);
}

TEST_CASE("Test log Levels", "[ttl1]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");
  logger->log_info("hello world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [info] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels debug", "[ttl2]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");
  logger->log_debug("hello world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [debug] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels trace", "[ttl3]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");

  logger->log_trace("hello world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [trace] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels error", "[ttl4]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");

  logger->log_error("hello world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels change", "[ttl5]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");

  logger->log_error("hello world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));
  oss.str("");
  oss.clear();
  REQUIRE(0 == oss.str().length());
  logger->setLogLevel("off");

  logger->log_error("hello world");

  REQUIRE(0 == oss.str().length());

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));

}


TEST_CASE("Test log LevelsConfigured", "[ttl6]") {
  std::ostringstream oss;

  std::shared_ptr<minifi::Configure> config = std::make_shared<minifi::Configure>();

  config->set(BaseLogger::nifi_log_appender, "OutputStreamAppender");
  config->set(
      org::apache::nifi::minifi::core::logging::OutputStreamAppender::nifi_log_output_stream_error_stderr,
      "true");

  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();

  auto oldrdbuf = std::cerr.rdbuf();
  std::cerr.rdbuf(oss.rdbuf());

  std::unique_ptr<BaseLogger> newLogger = LogInstance::getConfiguredLogger(
      config);

  logger->updateLogger(std::move(newLogger));

  logger->setLogLevel("trace");

  // capture stderr
  logger->log_error("hello world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));

  std::cerr.rdbuf(oldrdbuf);

  config->set(BaseLogger::nifi_log_appender, "nullappender");

  newLogger = LogInstance::getConfiguredLogger(config);

  logger->updateLogger(std::move(newLogger));

  oss.str("");
  oss.clear();
  REQUIRE(0 == oss.str().length());

  // should have nothing from the null appender
  logger->log_info("hello world");
  logger->log_debug("hello world");
  logger->log_trace("hello world");

  REQUIRE(0 == oss.str().length());

}

TEST_CASE("Test log Levels With std::string", "[ttl1]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");
  std::string world = "world";
  logger->log_error("hello %s", world);

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));
  oss.str("");
  oss.clear();
  REQUIRE(0 == oss.str().length());
  logger->setLogLevel("off");

  logger->log_error("hello world");

  REQUIRE(0 == oss.str().length());

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));

}

TEST_CASE("Test log Levels debug With std::string ", "[ttl2]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");
  std::string world = "world";
  logger->log_debug("hello %s", world);

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [debug] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels trace With std::string", "[ttl3]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");
  std::string world = "world";
  logger->log_trace("hello %s", world);

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [trace] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels error With std::string ", "[ttl4]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");

  std::string world = "world";
  logger->log_error("hello %s", world);

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));
}

TEST_CASE("Test log Levels change With std::string ", "[ttl5]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");

  std::string world = "world";
  logger->log_error("hello %s", world);

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));
  oss.str("");
  oss.clear();
  REQUIRE(0 == oss.str().length());
  logger->setLogLevel("off");

  logger->log_error("hello %s", world);

  REQUIRE(0 == oss.str().length());

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));

}

TEST_CASE("Test log Levels change With std::string maybe ", "[ttl5]") {
  std::ostringstream oss;

  std::unique_ptr<BaseLogger> outputLogger = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::OutputStreamAppender(oss,
                                                                         0));
  std::shared_ptr<logging::Logger> logger = logging::Logger::getLogger();
  logger->updateLogger(std::move(outputLogger));
  logger->setLogLevel("trace");

  logger->log_error("hello %s", "world");

  REQUIRE(
      true
          == contains(
              oss.str(),
              "[minifi log -- org::apache::nifi::minifi::core::logging::OutputStreamAppender] [error] hello world"));
  oss.str("");
  oss.clear();
  REQUIRE(0 == oss.str().length());
  logger->setLogLevel("off");

  logger->log_error("hello %s", "world");

  REQUIRE(0 == oss.str().length());

  std::unique_ptr<BaseLogger> nullAppender = std::unique_ptr<BaseLogger>(
      new org::apache::nifi::minifi::core::logging::NullAppender());

  logger->updateLogger(std::move(nullAppender));

}
