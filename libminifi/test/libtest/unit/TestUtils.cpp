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

#include "TestUtils.h"

#include <type_traits>

#ifdef WIN32
#include <windows.h>
#include <aclapi.h>
#endif

#include "minifi-cpp/utils/gsl.h"

#ifdef WIN32
namespace {

void setAclOnFileOrDirectory(std::string file_name, DWORD perms, ACCESS_MODE perm_options) {
  PSECURITY_DESCRIPTOR security_descriptor = nullptr;
  const auto security_descriptor_deleter = gsl::finally([&security_descriptor] { if (security_descriptor) { LocalFree((HLOCAL) security_descriptor); } });

  PACL old_acl = nullptr;  // GetNamedSecurityInfo will set this to a non-owning pointer to a field inside security_descriptor: no need to free it
  if (GetNamedSecurityInfo(file_name.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &old_acl, NULL, &security_descriptor) != ERROR_SUCCESS) {
    throw std::runtime_error("Could not get security info for file: " + file_name);
  }

  char trustee_name[] = "Everyone";
  EXPLICIT_ACCESS explicit_access = {
    .grfAccessPermissions = perms,
    .grfAccessMode = perm_options,
    .grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
    .Trustee = { .TrusteeForm = TRUSTEE_IS_NAME, .ptstrName = trustee_name }
  };

  PACL new_acl = nullptr;
  const auto new_acl_deleter = gsl::finally([&new_acl] { if (new_acl) { LocalFree((HLOCAL) new_acl); } });

  if (SetEntriesInAcl(1, &explicit_access, old_acl, &new_acl) != ERROR_SUCCESS) {
    throw std::runtime_error("Could not create new ACL for file: " + file_name);
  }

  if (SetNamedSecurityInfo(file_name.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, new_acl, NULL) != ERROR_SUCCESS) {
    throw std::runtime_error("Could not set the new ACL for file: " + file_name);
  }
}

}  // namespace
#endif

namespace org::apache::nifi::minifi::test::utils {

std::filesystem::path putFileToDir(const std::filesystem::path& dir_path, const std::filesystem::path& file_name, const std::string& content) {
  auto file_path = dir_path/file_name;
  std::ofstream out_file(file_path, std::ios::binary | std::ios::out);
  assert(out_file.is_open());
  out_file << content;
  return file_path;
}

std::string getFileContent(const std::filesystem::path& file_name) {
  std::ifstream file_handle(file_name, std::ios::binary | std::ios::in);
  assert(file_handle.is_open());
  std::string file_content{ (std::istreambuf_iterator<char>(file_handle)), (std::istreambuf_iterator<char>()) };
  return file_content;
}

void makeFileOrDirectoryNotWritable(const std::filesystem::path& file_name) {
#ifdef WIN32
  setAclOnFileOrDirectory(file_name.string(), FILE_GENERIC_WRITE, DENY_ACCESS);
#else
  std::filesystem::permissions(file_name, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);
#endif
}

void makeFileOrDirectoryWritable(const std::filesystem::path& file_name) {
#ifdef WIN32
  setAclOnFileOrDirectory(file_name.string(), FILE_GENERIC_WRITE, GRANT_ACCESS);
#else
  std::filesystem::permissions(file_name, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
#endif
}

void ManualClock::advance(std::chrono::milliseconds elapsed_time) {
  if (elapsed_time.count() < 0) {
    throw std::logic_error("A steady clock can only be advanced forward");
  }
  std::lock_guard lock(mtx_);
  time_ += elapsed_time;
  for (auto* cv : cvs_) {
    cv->notify_all();
  }
}

bool ManualClock::wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lck, std::chrono::milliseconds time, const std::function<bool()>& pred) {
  std::chrono::milliseconds now;
  {
    std::unique_lock lock(mtx_);
    now = time_;
    cvs_.insert(&cv);
  }
  cv.wait_for(lck, time - now, [&] {
    now = timeSinceEpoch();
    return now >= time || pred();
  });
  {
    std::unique_lock lock(mtx_);
    cvs_.erase(&cv);
  }
  return pred();
}

void matchJSON(const internal::JsonContext& ctx, const rapidjson::Value& actual, const rapidjson::Value& expected, bool strict) {
  if (expected.IsObject()) {
    REQUIRE_WARN(actual.IsObject(), fmt::format("Expected object at {}", ctx.path()));
    for (const auto& expected_member : expected.GetObject()) {
      std::string_view name{expected_member.name.GetString(), expected_member.name.GetStringLength()};
      REQUIRE_WARN(actual.HasMember(expected_member.name), fmt::format("Expected member '{}' at {}", name, ctx.path()));
      matchJSON(internal::JsonContext{.parent = &ctx, .member = name}, actual[expected_member.name], expected_member.value, strict);
    }
    if (strict) {
      for (const auto& actual_member : actual.GetObject()) {
        std::string_view name{actual_member.name.GetString(), actual_member.name.GetStringLength()};
        REQUIRE_WARN(expected.HasMember(actual_member.name), fmt::format("Did not expect member '{}' at {}", name, ctx.path()));
      }
    }
  } else if (expected.IsArray()) {
    REQUIRE_WARN(actual.IsArray(), fmt::format("Expected array at {}", ctx.path()));
    REQUIRE_WARN(actual.Size() == expected.Size(), fmt::format("Expected array of length {}, got {} at {}", expected.Size(), actual.Size(), ctx.path()));
    for (rapidjson::SizeType idx{0}; idx < expected.Size(); ++idx) {
      matchJSON(internal::JsonContext{.parent = &ctx, .member = std::to_string(idx)}, actual[idx], expected[idx], strict);
    }
  } else {
    REQUIRE_WARN(actual == expected, fmt::format("Values are not equal at {}", ctx.path()));
  }
}

void verifyJSON(const std::string& actual_str, const std::string& expected_str, bool strict) {
  rapidjson::Document actual;
  rapidjson::Document expected;
  REQUIRE_FALSE(actual.Parse(actual_str.c_str()).HasParseError());
  REQUIRE_FALSE(expected.Parse(expected_str.c_str()).HasParseError());

  matchJSON(internal::JsonContext{}, actual, expected, strict);
}

bool countLogOccurrencesUntil(const std::string& pattern,
                              const size_t occurrences,
                              const std::chrono::milliseconds max_duration,
                              const std::chrono::milliseconds wait_time) {
  auto start_time = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < start_time + max_duration) {
    if (LogTestController::getInstance().countOccurrences(pattern) == occurrences)
      return true;
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

std::error_code sendMessagesViaTCP(const std::vector<std::string_view>& contents, const asio::ip::tcp::endpoint& remote_endpoint, const std::optional<std::string_view> delimiter) {
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::error_code err;
  std::ignore = socket.connect(remote_endpoint, err);
  if (err)
    return err;
  for (auto& content : contents) {
    std::string tcp_message(content);
    if (delimiter)
      tcp_message += *delimiter;
    asio::write(socket, asio::buffer(tcp_message, tcp_message.size()), err);
    if (err)
      return err;
  }
  return {};
}

std::expected<asio::ip::udp::endpoint, std::error_code> sendUdpDatagram(const asio::const_buffer content, const asio::ip::udp::endpoint& remote_endpoint) {
  asio::io_context io_context;
  asio::ip::udp::socket socket(io_context);
  std::error_code err;
  std::ignore = socket.open(remote_endpoint.protocol(), err);
  if (err) {
    return std::unexpected{err};
  }
  socket.send_to(content, remote_endpoint, 0, err);
  if (err) {
    return std::unexpected{err};
  }
  return socket.local_endpoint();
}

std::expected<asio::ip::udp::endpoint, std::error_code> sendUdpDatagram(const std::span<std::byte const> content, const asio::ip::udp::endpoint& remote_endpoint) {
  return sendUdpDatagram(asio::const_buffer(content.data(), content.size()), remote_endpoint);
}

std::expected<asio::ip::udp::endpoint, std::error_code> sendUdpDatagram(const std::string_view content, const asio::ip::udp::endpoint& remote_endpoint) {
  return sendUdpDatagram(asio::buffer(content), remote_endpoint);
}

bool isIPv6Disabled() {
  asio::io_context io_context;
  std::error_code error_code;
  asio::ip::tcp::socket socket_tcp(io_context);
  std::ignore = socket_tcp.connect(asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), 10), error_code);
  return error_code.value() == EADDRNOTAVAIL;
}


std::error_code sendMessagesViaSSL(const std::vector<std::string_view>& contents,
    const asio::ip::tcp::endpoint& remote_endpoint,
    const std::filesystem::path& ca_cert_path,
    const std::optional<minifi::utils::net::SslData>& ssl_data,
    asio::ssl::context::method method) {
  asio::ssl::context ctx(method);
  ctx.load_verify_file(ca_cert_path.string());
  if (ssl_data) {
    ctx.set_verify_mode(asio::ssl::verify_peer);
    ctx.use_certificate_file(ssl_data->cert_loc.string(), asio::ssl::context::pem);
    ctx.use_private_key_file(ssl_data->key_loc.string(), asio::ssl::context::pem);
    ctx.set_password_callback([password = ssl_data->key_pw](std::size_t&, asio::ssl::context_base::password_purpose&) { return password; });
  }
  asio::io_context io_context;
  asio::ssl::stream<asio::ip::tcp::socket> socket(io_context, ctx);
  auto shutdown_socket = gsl::finally([&] {
    asio::error_code ec;
    std::ignore = socket.lowest_layer().cancel(ec);
    std::ignore = socket.shutdown(ec);
  });
  asio::error_code err;
  std::ignore = socket.lowest_layer().connect(remote_endpoint, err);
  if (err) {
    return err;
  }
  std::ignore = socket.handshake(asio::ssl::stream_base::client, err);
  if (err) {
    return err;
  }
  for (auto& content : contents) {
    std::string tcp_message(content);
    tcp_message += '\n';
    asio::write(socket, asio::buffer(tcp_message, tcp_message.size()), err);
    if (err) {
      return err;
    }
  }
  return {};
}

std::vector<LogMessageView> extractLogMessageViews(const std::string& log_str) {
  std::vector<LogMessageView> messages;
  const std::regex header_pattern(R"((\[[\d\-\s\:\.]+\]) (\s*\[[^\]]+\]) \[(.*)\])");
  struct HeaderMarker {
    size_t start;
    std::string_view timestamp;
    std::string_view logger_class;
    std::string_view log_level;
    size_t end;
  };

  std::vector<HeaderMarker> markers = ranges::subrange<std::sregex_iterator>(std::sregex_iterator(log_str.begin(), log_str.end(), header_pattern),
                                       std::sregex_iterator()) |
      ranges::views::transform([=](const std::smatch& m) {
        return HeaderMarker{.start = static_cast<size_t>(m.position(0)),
            .timestamp = std::string_view{log_str.data() + m.position(1), static_cast<size_t>(m.length(1))},
            .logger_class = std::string_view{log_str.data() + m.position(2), static_cast<size_t>(m.length(2))},
            .log_level = std::string_view{log_str.data() + m.position(3), static_cast<size_t>(m.length(3))},
            .end = static_cast<size_t>(m.position(0) + m.length(0))
        };
      }) | ranges::to<std::vector>();

  markers.push_back(HeaderMarker{.start = log_str.size(),
      .timestamp = {},
      .logger_class = {},
      .log_level = {},
      .end = log_str.size()
  });

  for (auto window: markers | ranges::views::sliding(2)) {
    messages.push_back(LogMessageView{.timestamp = window[0].timestamp,
      .logger_class = window[0].logger_class,
      .log_level = window[0].log_level,
      .payload = {log_str.data() + window[0].end, window[1].start - window[0].end}});
  }

  return messages;
}

}  // namespace org::apache::nifi::minifi::test::utils
