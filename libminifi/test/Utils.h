/**
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

#include <string>
#include <utility>
#include <vector>

#include "rapidjson/document.h"
#include "asio.hpp"
#include "asio/ssl.hpp"
#include "net/Ssl.h"

using namespace std::chrono_literals;

#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#include "Connection.h"
#include "FlowFileQueue.h"
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

// carries out a loose match on objects, i.e. it doesn't matter if the
// actual object has extra fields than expected
void matchJSON(const rapidjson::Value& actual, const rapidjson::Value& expected) {
  if (expected.IsObject()) {
    REQUIRE(actual.IsObject());
    for (const auto& expected_member : expected.GetObject()) {
      REQUIRE(actual.HasMember(expected_member.name));
      matchJSON(actual[expected_member.name], expected_member.value);
    }
  } else if (expected.IsArray()) {
    REQUIRE(actual.IsArray());
    REQUIRE(actual.Size() == expected.Size());
    for (size_t idx{0}; idx < expected.Size(); ++idx) {
      matchJSON(actual[idx], expected[idx]);
    }
  } else {
    REQUIRE(actual == expected);
  }
}

void verifyJSON(const std::string& actual_str, const std::string& expected_str, bool strict = false) {
  rapidjson::Document actual, expected;
  REQUIRE_FALSE(actual.Parse(actual_str.c_str()).HasParseError());
  REQUIRE_FALSE(expected.Parse(expected_str.c_str()).HasParseError());

  if (strict) {
    REQUIRE(actual == expected);
  } else {
    matchJSON(actual, expected);
  }
}

template<typename T>
class ExceptionSubStringMatcher : public Catch::MatcherBase<T> {
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
                              const std::chrono::milliseconds wait_time = 50ms) {
  auto start_time = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < start_time + max_duration) {
    if (LogTestController::getInstance().countOccurrences(pattern) == occurrences)
      return true;
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

bool sendMessagesViaTCP(const std::vector<std::string_view>& contents, const asio::ip::tcp::endpoint& remote_endpoint) {
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  socket.connect(remote_endpoint);
  std::error_code err;
  for (auto& content : contents) {
    std::string tcp_message(content);
    tcp_message += '\n';
    asio::write(socket, asio::buffer(tcp_message, tcp_message.size()), err);
    if (err) {
      return false;
    }
  }
  return true;
}

bool sendUdpDatagram(const asio::const_buffer content, const asio::ip::udp::endpoint& remote_endpoint) {
  asio::io_context io_context;
  asio::ip::udp::socket socket(io_context);
  socket.open(remote_endpoint.protocol());
  std::error_code err;
  socket.send_to(content, remote_endpoint, 0, err);
  return !err;
}

bool sendUdpDatagram(const gsl::span<std::byte const> content, const asio::ip::udp::endpoint& remote_endpoint) {
  return sendUdpDatagram(asio::const_buffer(content.begin(), content.size()), remote_endpoint);
}

bool sendUdpDatagram(const std::string_view content, const asio::ip::udp::endpoint& remote_endpoint) {
  return sendUdpDatagram(asio::buffer(content), remote_endpoint);
}

bool isIPv6Disabled() {
  asio::io_context io_context;
  std::error_code error_code;
  asio::ip::tcp::socket socket_tcp(io_context);
  socket_tcp.connect(asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), 10), error_code);
  return error_code.value() == EADDRNOTAVAIL;
}

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

bool sendMessagesViaSSL(const std::vector<std::string_view>& contents,
                        const asio::ip::tcp::endpoint& remote_endpoint,
                        const std::string& ca_cert_path,
                        const std::optional<minifi::utils::net::SslData>& ssl_data = std::nullopt) {
  asio::ssl::context ctx(asio::ssl::context::sslv23);
  ctx.load_verify_file(ca_cert_path);
  if (ssl_data) {
    ctx.set_verify_mode(asio::ssl::verify_peer);
    ctx.use_certificate_file(ssl_data->cert_loc, asio::ssl::context::pem);
    ctx.use_private_key_file(ssl_data->key_loc, asio::ssl::context::pem);
    ctx.set_password_callback([password = ssl_data->key_pw](std::size_t&, asio::ssl::context_base::password_purpose&) { return password; });
  }
  asio::io_context io_context;
  asio::ssl::stream<asio::ip::tcp::socket> socket(io_context, ctx);
  asio::error_code err;
  socket.lowest_layer().connect(remote_endpoint, err);
  if (err) {
    return false;
  }
  socket.handshake(asio::ssl::stream_base::client, err);
  if (err) {
    return false;
  }
  for (auto& content : contents) {
    std::string tcp_message(content);
    tcp_message += '\n';
    asio::write(socket, asio::buffer(tcp_message, tcp_message.size()), err);
    if (err) {
      return false;
    }
  }
  return true;
}

}  // namespace org::apache::nifi::minifi::test::utils
