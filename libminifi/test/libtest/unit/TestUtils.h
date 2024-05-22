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
#pragma once

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "utils/file/FileUtils.h"
#include "utils/Id.h"
#include "utils/TimeUtil.h"
#include "TestBase.h"

#include "rapidjson/document.h"
#include "asio.hpp"
#include "asio/ssl.hpp"
#include "utils/net/Ssl.h"

using namespace std::literals::chrono_literals;

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#include "Connection.h"
#include "utils/FlowFileQueue.h"
#include "Catch.h"

#define FIELD_ACCESSOR(field) \
  template<typename T> \
  static auto get_##field(T&& instance) -> decltype((std::forward<T>(instance).field)) { \
    return std::forward<T>(instance).field; \
  }

#define METHOD_ACCESSOR(method) \
  template<typename T, typename ...Args> \
  static auto call_##method(T&& instance, Args&& ...args) -> decltype((std::forward<T>(instance).method(std::forward<Args>(args)...))) { \
    return std::forward<T>(instance).method(std::forward<Args>(args)...); \
  }

namespace org::apache::nifi::minifi::test::utils {

std::filesystem::path putFileToDir(const std::filesystem::path& dir_path, const std::filesystem::path& file_name, const std::string& content);
std::string getFileContent(const std::filesystem::path& file_name);

void makeFileOrDirectoryNotWritable(const std::filesystem::path& file_name);
void makeFileOrDirectoryWritable(const std::filesystem::path& file_name);

inline minifi::utils::Identifier generateUUID() {
  // TODO(hunyadi): Will make the Id generator manage lifetime using a unique_ptr and return a raw ptr on access
  static std::shared_ptr<minifi::utils::IdGenerator> id_generator = minifi::utils::IdGenerator::getIdGenerator();
  return id_generator->generate();
}

class ManualClock : public minifi::utils::timeutils::SteadyClock {
 public:
  [[nodiscard]] std::chrono::milliseconds timeSinceEpoch() const override {
    std::lock_guard lock(mtx_);
    return time_;
  }

  [[nodiscard]] std::chrono::time_point<std::chrono::steady_clock> now() const override {
    return std::chrono::steady_clock::time_point{timeSinceEpoch()};
  }

  void advance(std::chrono::milliseconds elapsed_time);
  bool wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lck, std::chrono::milliseconds time, const std::function<bool()>& pred) override;

 private:
  mutable std::mutex mtx_;
  std::unordered_set<std::condition_variable*> cvs_;
  std::chrono::milliseconds time_{0};
};

template <class Rep, class Period, typename Fun>
bool verifyEventHappenedInPollTime(
    const std::chrono::duration<Rep, Period>& wait_duration,
    Fun&& check,
    std::chrono::microseconds check_interval = std::chrono::milliseconds(100)) {
  std::chrono::steady_clock::time_point wait_end = std::chrono::steady_clock::now() + wait_duration;
  do {
    if (std::forward<Fun>(check)()) {
      return true;
    }
    std::this_thread::sleep_for(check_interval);
  } while (std::chrono::steady_clock::now() < wait_end);
  return false;
}

template <class Rep, class Period, typename ...String>
bool verifyLogLinePresenceInPollTime(const std::chrono::duration<Rep, Period>& wait_duration, String&&... patterns) {
  auto check = [&patterns...] {
    const std::string logs = LogTestController::getInstance().getLogs();
    return ((logs.find(patterns) != std::string::npos) && ...);
  };
  return verifyEventHappenedInPollTime(wait_duration, check);
}

template <class Rep, class Period, typename ...String>
bool verifyLogLineVariantPresenceInPollTime(const std::chrono::duration<Rep, Period>& wait_duration, String&&... patterns) {
  auto check = [&patterns...] {
    const std::string logs = LogTestController::getInstance().getLogs();
    return ((logs.find(patterns) != std::string::npos) || ...);
  };
  return verifyEventHappenedInPollTime(wait_duration, check);
}

namespace internal {
struct JsonContext {
  const JsonContext *parent{nullptr};
  std::string_view member;

  std::string path() const {
    if (!parent) {
      return "/";
    }
    return minifi::utils::string::join_pack(parent->path(), member, "/");
  }
};
}  // namespace internal

#define REQUIRE_WARN(cond, msg) if (!(cond)) {WARN(msg); REQUIRE(cond);}

// carries out a loose match on objects, i.e. it doesn't matter if the
// actual object has extra fields than expected
void matchJSON(const internal::JsonContext& ctx, const rapidjson::Value& actual, const rapidjson::Value& expected, bool strict = false);
void verifyJSON(const std::string& actual_str, const std::string& expected_str, bool strict = false);

template<typename T>
class ExceptionSubStringMatcher : public Catch::Matchers::MatcherBase<T> {
 public:
  explicit ExceptionSubStringMatcher(std::vector<std::string> exception_substr) :
      possible_exception_substrs_(std::move(exception_substr)) {}

  bool match(T const& script_exception) const override {
    for (auto& possible_exception_substr : possible_exception_substrs_) {
      if (std::string(script_exception.what()).find(possible_exception_substr) != std::string::npos)
        return true;
    }
    return false;
  }

  std::string describe() const override { return "Checks whether the exception message contains at least one of the provided exception substrings"; }

 private:
  std::vector<std::string> possible_exception_substrs_;
};

bool countLogOccurrencesUntil(const std::string& pattern,
                              const int occurrences,
                              const std::chrono::milliseconds max_duration,
                              const std::chrono::milliseconds wait_time = 50ms);
std::error_code sendMessagesViaTCP(const std::vector<std::string_view>& contents, const asio::ip::tcp::endpoint& remote_endpoint, const std::optional<std::string_view> delimiter = std::nullopt);
std::error_code sendUdpDatagram(const asio::const_buffer content, const asio::ip::udp::endpoint& remote_endpoint);

std::error_code sendUdpDatagram(const std::span<std::byte const> content, const asio::ip::udp::endpoint& remote_endpoint);
std::error_code sendUdpDatagram(const std::string_view content, const asio::ip::udp::endpoint& remote_endpoint);

bool isIPv6Disabled();

struct ConnectionTestAccessor {
  FIELD_ACCESSOR(queue_);
};

struct FlowFileQueueTestAccessor {
  FIELD_ACCESSOR(min_size_);
  FIELD_ACCESSOR(max_size_);
  FIELD_ACCESSOR(target_size_);
  FIELD_ACCESSOR(clock_);
  FIELD_ACCESSOR(swapped_flow_files_);
  FIELD_ACCESSOR(load_task_);
  FIELD_ACCESSOR(queue_);
};

std::error_code sendMessagesViaSSL(const std::vector<std::string_view>& contents,
    const asio::ip::tcp::endpoint& remote_endpoint,
    const std::filesystem::path& ca_cert_path,
    const std::optional<minifi::utils::net::SslData>& ssl_data = std::nullopt,
    asio::ssl::context::method method = asio::ssl::context::tls_client);

#ifdef WIN32
inline std::error_code hide_file(const std::filesystem::path& file_name) {
  const bool success = SetFileAttributesA(file_name.string().c_str(), FILE_ATTRIBUTE_HIDDEN);
  if (!success) {
    // note: All possible documented error codes from GetLastError are in [0;15999] at the time of writing.
    // The below casting is safe in [0;std::numeric_limits<int>::max()], int max is guaranteed to be at least 32767
    return { static_cast<int>(GetLastError()), std::system_category() };
  }
  return {};
}
#endif /* WIN32 */

template<typename T>
concept NetworkingProcessor = std::derived_from<T, minifi::core::Processor>
    && requires(T x) {
      {T::Port} -> std::convertible_to<core::PropertyReference>;
      {x.getPort()} -> std::convertible_to<uint16_t>;
    };  // NOLINT(readability/braces)

template<NetworkingProcessor T>
uint16_t scheduleProcessorOnRandomPort(const std::shared_ptr<TestPlan>& test_plan, const std::shared_ptr<T>& processor) {
  REQUIRE(processor->setProperty(T::Port, "0"));
  test_plan->scheduleProcessor(processor);
  REQUIRE(verifyEventHappenedInPollTime(250ms, [&processor] { return processor->getPort() != 0; }, 20ms));
  return processor->getPort();
}

inline bool runningAsUnixRoot() {
#ifdef WIN32
  return false;
#else
  return geteuid() == 0;
#endif
}
}  // namespace org::apache::nifi::minifi::test::utils
