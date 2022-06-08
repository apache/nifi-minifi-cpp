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
#include "../Catch.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/ZlibStream.h"
#include "StreamPipe.h"
#include "utils/IntegrationTestUtils.h"

using namespace std::literals::chrono_literals;

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

struct CStringConvertible {
  [[nodiscard]] const char* c_str() const {
    return data.c_str();
  }

  std::string data;
};

TEST_CASE("Test log custom string formatting", "[ttl6]") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_trace("%s %s %s", "one", std::string{"two"}, CStringConvertible{"three"});

  REQUIRE(LogTestController::getInstance().contains("[trace] one two three", 0s));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log lazy string generation", "[ttl7]") {
  LogTestController::getInstance().setDebug<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  int call_count = 0;

  logger->log_trace("%s", [&] {
    ++call_count;
    return std::string{"hello trace"};
  });

  logger->log_debug("%s", [&] {
    ++call_count;
    return std::string{"hello debug"};
  });

  REQUIRE(LogTestController::getInstance().contains("[debug] hello debug", 0s));
  REQUIRE(call_count == 1);
  LogTestController::getInstance().reset();
}

namespace single {
class TestClass {
};
}  // namespace single

class TestClass2 {
};

TEST_CASE("Test ShortenNames", "[ttl8]") {
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

std::string decompress(const std::shared_ptr<InputStream>& input) {
  auto output = std::make_unique<BufferStream>();
  auto decompressor = std::make_shared<ZlibDecompressStream>(gsl::make_not_null(output.get()));
  minifi::internal::pipe(*input, *decompressor);
  decompressor->close();
  return utils::span_to<std::string>(output->getBuffer().as_span<const char>());
}

TEST_CASE("Test Compression", "[ttl9]") {
  auto& log_config = logging::LoggerConfiguration::getConfiguration();
  auto properties = std::make_shared<logging::LoggerProperties>();
  std::string className;
  SECTION("Using root logger") {
    className = "CompressionTestClassUsingRoot";
    // by default the root logger is OFF
    properties->set("logger.root", "INFO");
  }
  SECTION("Inherit compression sink") {
    className = "CompressionTestClassInheriting";
    properties->set("appender.null", "null");
    properties->set("logger." + className, "INFO,null");
  }
  log_config.initialize(properties);
  auto logger = log_config.getLogger(className);
  logger->log_error("Hi there");
  std::shared_ptr<InputStream> compressed_log{logging::LoggerConfiguration::getCompressedLog(true)};
  REQUIRE(compressed_log);
  auto logs = decompress(compressed_log);
  REQUIRE(logs.find("Hi there") != std::string::npos);
}

class LoggerTestAccessor {
 public:
  static void setCompressionCacheSegmentSize(logging::LoggerConfiguration& log_config, size_t value) {
    log_config.compression_manager_.cache_segment_size = value;
  }
  static void setCompressionCompressedSegmentSize(logging::LoggerConfiguration& log_config, size_t value) {
    log_config.compression_manager_.compressed_segment_size = value;
  }
  static size_t getUncompressedSize(logging::LoggerConfiguration& log_config) {
    return log_config.compression_manager_.getSink()->cached_logs_.size();
  }
  static size_t getCompressedSize(logging::LoggerConfiguration& log_config) {
    return log_config.compression_manager_.getSink()->compressed_logs_.size();
  }
};

TEST_CASE("Test Compression cache overflow is discarded intermittently", "[ttl10]") {
  auto& log_config = logging::LoggerConfiguration::getConfiguration();
  auto properties = std::make_shared<logging::LoggerProperties>();
  properties->set(logging::internal::CompressionManager::compression_cached_log_max_size_, "10 KB");
  LoggerTestAccessor::setCompressionCacheSegmentSize(log_config, 1_KiB);
  std::string className = "CompressionTestCacheCleaned";
  // by default the root logger is OFF
  properties->set("logger.root", "INFO");
  log_config.initialize(properties);
  auto logger = log_config.getLogger(className);
  for (size_t idx = 0; idx < 10000; ++idx) {
    logger->log_error("Hi there");
  }
  bool cache_shrunk = utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, [&] {
    return LoggerTestAccessor::getUncompressedSize(log_config) <= 10_KiB;
  });
  REQUIRE(cache_shrunk);
}

TEST_CASE("Setting either properties to 0 disables in-memory compressed logs", "[ttl11]") {
  auto& log_config = logging::LoggerConfiguration::getConfiguration();
  auto properties = std::make_shared<logging::LoggerProperties>();
  bool is_nullptr = false;
  SECTION("Cached log size is set to 0") {
    is_nullptr = true;
    properties->set(logging::internal::CompressionManager::compression_cached_log_max_size_, "0");
  }
  SECTION("Compressed log size is set to 0") {
    is_nullptr = true;
    properties->set(logging::internal::CompressionManager::compression_compressed_log_max_size_, "0");
  }
  SECTION("Sanity check") {
    is_nullptr = false;
    // pass
  }
  // by default the root logger is OFF
  properties->set("logger.root", "INFO");
  log_config.initialize(properties);
  auto logger = log_config.getLogger("DisableCompressionTestLogger");
  logger->log_error("Hi there");
  REQUIRE((logging::LoggerConfiguration::getCompressedLog(true) == nullptr) == is_nullptr);
}
