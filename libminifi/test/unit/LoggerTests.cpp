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
#include <random>
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/logging/LoggerConfiguration.h"
#include "io/ZlibStream.h"
#include "io/StreamPipe.h"
#include "unit/TestUtils.h"
#include "utils/span.h"
#include "utils/net/AsioSocketUtils.h"

using namespace std::literals::chrono_literals;

TEST_CASE("Test log Levels", "[ttl1]") {
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_info("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [info] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels debug", "[ttl2]") {
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_debug("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [debug] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels trace", "[ttl3]") {
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_trace("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [trace] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels error", "[ttl4]") {
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_error("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log Levels change", "[ttl5]") {
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_error("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  LogTestController::getInstance().reset();
  LogTestController::getInstance().setOff<logging::Logger>();
  logger->log_error("hello {}", "world");

  REQUIRE(false == LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world", std::chrono::seconds(0)));
  LogTestController::getInstance().reset();
}

TEST_CASE("Logger configured with an ID prints this ID in every log line", "[logger][id]") {
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  const auto uuid = utils::IdGenerator::getIdGenerator()->generate();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger(uuid);
  logger->log_error("hello {}", "world");

  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world (" + uuid.to_string() + ")"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Printing of the ID can be disabled in the config", "[logger][id][configuration]") {
  auto properties = std::make_shared<logging::LoggerProperties>();

  bool id_is_present{};
  SECTION("Property not set") {
    id_is_present = true;
  }
  SECTION("Property set to true") {
    properties->set("logger.include.uuid", "true");
    id_is_present = true;
  }
  SECTION("Property set to false") {
    properties->set("logger.include.uuid", "false");
    id_is_present = false;
  }

  const auto uuid = utils::IdGenerator::getIdGenerator()->generate();
  std::shared_ptr<logging::Logger> logger = LogTestController::getInstance(properties)->getLogger<logging::Logger>(uuid);
  logger->log_error("hello {}", "world");
  CHECK(LogTestController::getInstance(properties)->contains("[org::apache::nifi::minifi::core::logging::Logger] [error] hello world"));
  CHECK(id_is_present == LogTestController::getInstance(properties)->contains(uuid.to_string()));
}

struct CStringConvertible {
  [[nodiscard]] const char* c_str() const {
    return data.c_str();
  }

  std::string data;
};

TEST_CASE("Test log custom string formatting", "[ttl6]") {
  LogTestController::getInstance().reset();
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_trace("{} {} {}", "one", std::string{"two"}, CStringConvertible{"three"}.c_str());

  REQUIRE(LogTestController::getInstance().contains("[trace] one two three", 0s));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test log lazy string generation", "[ttl7]") {
  LogTestController::getInstance().reset();
  LogTestController::getInstance().clear();
  LogTestController::getInstance().setDebug<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  int call_count = 0;

  logger->log_trace("{}", [&] {
    ++call_count;
    return std::string{"hello trace"};
  });

  logger->log_debug("{}", [&] {
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
  LogTestController::getInstance().clear();
  std::shared_ptr<logging::LoggerProperties> props = std::make_shared<logging::LoggerProperties>();

  props->set("spdlog.shorten_names", "true");

  std::shared_ptr<logging::Logger> logger = LogTestController::getInstance(props)->getLogger<logging::Logger>();
  logger->log_error("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance(props)->contains("[o::a::n::m::c::l::Logger] [error] hello world"));

  logger = LogTestController::getInstance(props)->getLogger<single::TestClass>();
  logger->log_error("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance(props)->contains("[s::TestClass] [error] hello world"));

  logger = LogTestController::getInstance(props)->getLogger<TestClass2>();
  logger->log_error("hello {}", "world");

  REQUIRE(true == LogTestController::getInstance(props)->contains("[TestClass2] [error] hello world"));

  LogTestController::getInstance(props)->reset();
  LogTestController::getInstance().reset();

  LogTestController::getInstance(props)->reset();
  LogTestController::getInstance().reset();
}

std::string decompress(const std::unique_ptr<minifi::io::InputStream>& input) {
  input->seek(0);
  auto output = std::make_unique<minifi::io::BufferStream>();
  auto decompressor = std::make_shared<minifi::io::ZlibDecompressStream>(gsl::make_not_null(output.get()));
  minifi::internal::pipe(*input, *decompressor);
  decompressor->close();
  return utils::span_to<std::string>(utils::as_span<const char>(output->getBuffer()));
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
  static bool waitForCompressionToHappen(logging::LoggerConfiguration& log_config) {
    auto sink = log_config.compression_manager_.getSink();
    return !sink || minifi::test::utils::verifyEventHappenedInPollTime(1s, [&]{ return sink->cached_logs_.itemCount() == 0; });
  }
  static auto getCompressedLogs(logging::LoggerConfiguration& log_config) {
    return log_config.compression_manager_.getCompressedLogs(std::chrono::milliseconds{0});
  }
};

TEST_CASE("Test Compression", "[ttl9]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
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
  log_config->initialize(properties);
  auto logger = log_config->getLogger(className);
  logger->log_error("Hi there");

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.size() == 1);
  auto logs = decompress(compressed_logs[0]);
  REQUIRE(logs.find("Hi there") != std::string::npos);
}

TEST_CASE("Test Compression cache overflow is discarded intermittently", "[ttl10]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  auto properties = std::make_shared<logging::LoggerProperties>();
  properties->set(logging::internal::CompressionManager::compression_cached_log_max_size_, "10 KB");
  LoggerTestAccessor::setCompressionCacheSegmentSize(*log_config, 1_KiB);
  std::string className = "CompressionTestCacheCleaned";
  // by default the root logger is OFF
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger(className);
  for (size_t idx = 0; idx < 10000; ++idx) {
    logger->log_error("Hi there");
  }
  bool cache_shrunk = minifi::test::utils::verifyEventHappenedInPollTime(std::chrono::seconds{1}, [&] {
    return LoggerTestAccessor::getUncompressedSize(*log_config) <= 10_KiB;
  });
  REQUIRE(cache_shrunk);
}

TEST_CASE("Setting either properties to 0 disables in-memory compressed logs", "[ttl11]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  auto properties = std::make_shared<logging::LoggerProperties>();
  bool is_empty = false;
  SECTION("Cached log size is set to 0") {
    is_empty = true;
    properties->set(logging::internal::CompressionManager::compression_cached_log_max_size_, "0");
  }
  SECTION("Compressed log size is set to 0") {
    is_empty = true;
    properties->set(logging::internal::CompressionManager::compression_compressed_log_max_size_, "0");
  }
  SECTION("Sanity check") {
    is_empty = false;
    // pass
  }
  // by default the root logger is OFF
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger("DisableCompressionTestLogger");
  logger->log_error("Hi there");

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.empty() == is_empty);
}

TEST_CASE("Setting max log entry length property trims long log entries", "[ttl12]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  auto properties = std::make_shared<logging::LoggerProperties>();
  properties->set("max.log.entry.length", "2");
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger("SetMaxLogEntryLengthTestLogger");
  logger->log_error("Hi there");

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.size() == 1);
  auto logs = decompress(compressed_logs[0]);
  REQUIRE(logs.find("Hi ") == std::string::npos);
  REQUIRE(logs.find("Hi") != std::string::npos);
}

TEST_CASE("Setting max log entry length property trims long formatted log entries", "[ttl13]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  auto properties = std::make_shared<logging::LoggerProperties>();
  properties->set("max.log.entry.length", "2");
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger("SetMaxLogEntryLengthInFormattedTestLogger");
  logger->log_error("Hi there {}", "John");

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.size() == 1);
  auto logs = decompress(compressed_logs[0]);
  REQUIRE(logs.find("Hi ") == std::string::npos);
  REQUIRE(logs.find("Hi") != std::string::npos);
}

TEST_CASE("Setting max log entry length to a size larger than the internal buffer size", "[ttl14]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  auto properties = std::make_shared<logging::LoggerProperties>();
  properties->set("max.log.entry.length", "1500");
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger("SetMaxLogEntryLengthToLargerThanBufferSizeTestLogger");
  std::string log(2000, 'a');
  std::string expected_log(1500, 'a');
  logger->log_error("{}", log);

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.size() == 1);
  auto logs = decompress(compressed_logs[0]);
  REQUIRE(logs.find(log) == std::string::npos);
  REQUIRE(logs.find(expected_log) != std::string::npos);
}

TEST_CASE("Setting max log entry length to unlimited results in unlimited log entry size", "[ttl15]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  auto properties = std::make_shared<logging::LoggerProperties>();
  std::string_view logger_name;
  SECTION("Use unlimited value") {
    properties->set("max.log.entry.length", "unlimited");
    logger_name = "SetMaxLogEntryLengthToUnlimitedTestLogger";
  }
  SECTION("Use -1 value") {
    properties->set("max.log.entry.length", "-1");
    logger_name = "SetMaxLogEntryLengthTo-1TestLogger";
  }
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger(logger_name);
  std::string log(5000, 'a');
  logger->log_error("{}", log);

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.size() == 1);
  auto logs = decompress(compressed_logs[0]);
  REQUIRE(logs.find(log) != std::string::npos);
  LogTestController::getInstance().reset();
}

TEST_CASE("fmt formatting works with the logger") {
  LogTestController::getInstance().setTrace<logging::Logger>();
  std::shared_ptr<logging::Logger> logger = logging::LoggerFactory<logging::Logger>::getLogger();
  logger->log_critical("{} equals to {}", 1min, 60s);
  logger->log_critical("{} in hex is {:#x}", 13, 13);
  logger->log_critical("Unix epoch: {}", std::chrono::system_clock::time_point());
  logger->log_critical("{:%Q %q} equals to {:%Q %q}", 2h, std::chrono::duration_cast<std::chrono::seconds>(2h));
  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [critical] 1m equals to 60s"));
  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [critical] 13 in hex is 0xd"));
  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [critical] Unix epoch: 1970-01-01 00:00:00.00"));
  CHECK(LogTestController::getInstance().contains("[org::apache::nifi::minifi::core::logging::Logger] [critical] 2 h equals to 7200 s"));

  LogTestController::getInstance().reset();
}

TEST_CASE("custom fmt formatter tests") {
  CHECK("abcde" == fmt::format("{}", utils::SmallString<5>{'a', 'b', 'c', 'd', 'e'}));
  CHECK("127.0.0.1:8080" == fmt::format("{}", asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 8080)));
}

std::vector<std::string> generateRandomStrings() {
  std::vector<std::string> random_strings;
  std::random_device rd;
  std::mt19937 eng(rd());
  constexpr const char * TEXT_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
  const int index_of_last_char = gsl::narrow<int>(strlen(TEXT_CHARS)) - 1;
  std::uniform_int_distribution<> distr(0, index_of_last_char);
  std::vector<char> data(100);
  const size_t SEGMENT_COUNT = 5;
  for (size_t idx = 0; idx < SEGMENT_COUNT; ++idx) {
    std::generate_n(data.begin(), data.size(), [&] { return TEXT_CHARS[static_cast<uint8_t>(distr(eng))]; });
    random_strings.push_back(std::string{data.begin(), data.end()} + "." + std::to_string(idx));
  }
  return random_strings;
}

TEST_CASE("Test sending multiple segments at once", "[ttl16]") {
  auto log_config = logging::LoggerConfiguration::newInstance();
  LoggerTestAccessor::setCompressionCompressedSegmentSize(*log_config, 100);
  LoggerTestAccessor::setCompressionCacheSegmentSize(*log_config, 100);
  auto properties = std::make_shared<logging::LoggerProperties>();
  // by default the root logger is OFF
  properties->set("logger.root", "INFO");
  log_config->initialize(properties);
  auto logger = log_config->getLogger("CompressionTestMultiSegment");

  const auto random_strings = generateRandomStrings();
  for (const auto& random_string : random_strings) {
    logger->log_error("{}", random_string);
  }

  REQUIRE(LoggerTestAccessor::waitForCompressionToHappen(*log_config));
  auto compressed_logs = LoggerTestAccessor::getCompressedLogs(*log_config);
  REQUIRE(compressed_logs.size() == random_strings.size());
  for (size_t i = 0; i < compressed_logs.size(); ++i) {
    auto logs = decompress(compressed_logs[i]);
    REQUIRE(logs.find(random_strings[i]) != std::string::npos);
  }
}
