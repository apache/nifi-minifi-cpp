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
#include <utility>
#include <string>
#include <memory>
#include <vector>
#include <ctime>
#include "../TestBase.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/ZlibStream.h"
#include "StreamPipe.h"

TEST_CASE("Test log Levels", "[ttl1]") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_info("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [info] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels debug", "[ttl2]") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_debug("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [debug] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels trace", "[ttl3]") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_trace("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [trace] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels error", "[ttl4]") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_error("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels change", "[ttl5]") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_error("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setOff<logging::Logger>();
  logger->log_error("hello %s", "world");

  REQUIRE(false == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world", std::chrono::seconds(0)));
  LogTestController::getInstance().reset();
}

namespace single {
class TestClass {
};
}  // namespace single

class TestClass2 {
};

TEST_CASE("Test ShortenNames", "[ttl6]") {
  std::shared_ptr<logging::LoggerProperties> props = std::make_shared<logging::LoggerProperties>();

  props->set("spdlog.shorten_names", "true");

  std::shared_ptr<logging::Logger> logger = LogTestController::getInstance(props)->getLogger<logging::Logger>();
  logger->log_error("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance(props)->contains("[o::a::n::m::c::l::Logger] [error] hello world"));

  logger = LogTestController::getInstance(props)->getLogger<single::TestClass>();
  logger->log_error("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance(props)->contains("[s::TestClass] [error] hello world"));

  logger = LogTestController::getInstance(props)->getLogger<TestClass2>();
  logger->log_error("hello %s", "world");

  REQUIRE(true == LogTestController::getInstance(props)->contains("[TestClass2] [error] hello world"));

  LogTestController::getInstance(props)->reset();
  LogTestController::getInstance().reset();

  LogTestController::getInstance(props)->reset();
  LogTestController::getInstance().reset();
}

using namespace minifi::io;

std::unique_ptr<BufferStream> decompress(const std::shared_ptr<InputStream>& input) {
  auto output = utils::make_unique<BufferStream>();
  auto decompressor = std::make_shared<ZlibDecompressStream>(gsl::make_not_null(output.get()));
  minifi::internal::pipe(input, decompressor);
  decompressor->close();
  return output;
}

class TestClass3 {};

TEST_CASE("Test Compression", "[ttl7]") {
  auto& log_config = logging::LoggerConfiguration::getConfiguration();
  auto properties = std::make_shared<logging::LoggerProperties>();
  // by default the root logger is OFF
  properties->set("logger.root", "INFO");
  log_config.initialize(properties);
  auto logger = logging::LoggerFactory<TestClass3>::getLogger();
  logger->log_error("Hi there");
  std::shared_ptr<InputStream> compressed_log{logging::LoggerConfiguration::getCompressedLog(true)};
  REQUIRE(compressed_log);
  auto log_buffer = decompress(compressed_log);
  std::string logs{reinterpret_cast<const char*>(log_buffer->getBuffer()), log_buffer->size()};
  REQUIRE(logs.find("Hi there") != std::string::npos);
}

class TestClass4 {};

TEST_CASE("Test Compression Sink is inherited", "[ttl7]") {
  auto& log_config = logging::LoggerConfiguration::getConfiguration();
  auto properties = std::make_shared<logging::LoggerProperties>();
  properties->set("logger.TestClass4", "INFO");
  log_config.initialize(properties);
  auto logger = logging::LoggerFactory<TestClass4>::getLogger();
  logger->log_error("Hi there TestClass4");
  std::shared_ptr<InputStream> compressed_log{logging::LoggerConfiguration::getCompressedLog(true)};
  REQUIRE(compressed_log);
  auto log_buffer = decompress(compressed_log);
  std::string logs{reinterpret_cast<const char*>(log_buffer->getBuffer()), log_buffer->size()};
  REQUIRE(logs.find("Hi there TestClass4") != std::string::npos);
}
