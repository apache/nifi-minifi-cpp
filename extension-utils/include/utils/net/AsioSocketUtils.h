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

#ifndef WIN32
#include <ifaddrs.h>
#endif

#include <string>
#include <utility>
#include <tuple>
#include <memory>

#include "asio/ssl.hpp"
#include "asio/ip/tcp.hpp"

#include "AsioCoro.h"
#include "utils/Hash.h"
#include "utils/StringUtils.h"  // for string <=> on libc++
#include "controllers/SSLContextService.h"
#include "io/BaseStream.h"
#include "utils/Deleters.h"
#include "utils/net/Socket.h"

namespace org::apache::nifi::minifi::utils::net {

using HandshakeType = asio::ssl::stream_base::handshake_type;
using TcpSocket = asio::ip::tcp::socket;
using SslSocket = asio::ssl::stream<asio::ip::tcp::socket>;

constexpr auto MINIFI_SSL_OPTIONS = asio::ssl::context::default_workarounds | asio::ssl::context::single_dh_use
    | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1;

class ConnectionId {
 public:
  ConnectionId(std::string hostname, std::string port) : hostname_(std::move(hostname)), service_(std::move(port)) {}
  ConnectionId(const ConnectionId& connection_id) = default;
  ConnectionId(ConnectionId&& connection_id) = default;
  ConnectionId& operator=(ConnectionId&&) = default;
  ConnectionId& operator=(const ConnectionId&) = default;

  auto operator<=>(const ConnectionId&) const = default;

  [[nodiscard]] std::string_view getHostname() const { return hostname_; }
  [[nodiscard]] std::string_view getService() const { return service_; }

 private:
  std::string hostname_;
  std::string service_;
};

template<class SocketType>
asio::awaitable<std::tuple<std::error_code>> handshake(SocketType&, asio::steady_timer::duration) = delete;
template<>
asio::awaitable<std::tuple<std::error_code>> handshake(TcpSocket&, asio::steady_timer::duration);
template<>
asio::awaitable<std::tuple<std::error_code>> handshake(SslSocket& socket, asio::steady_timer::duration);


asio::ssl::context getSslContext(const controllers::SSLContextService& ssl_context_service, asio::ssl::context::method ssl_context_method = asio::ssl::context::tls_client);

struct SocketData {
  std::string host = "localhost";
  int port = -1;
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service;
};

class AsioSocketConnection : public io::BaseStream {
 public:
  explicit AsioSocketConnection(SocketData socket_data);
  int initialize() override;
  size_t read(std::span<std::byte> out_buffer) override {
    gsl_Expects(stream_);
    return stream_->read(out_buffer);
  }
  size_t write(const uint8_t *in_buffer, size_t len) override {
    gsl_Expects(stream_);
    return stream_->write(in_buffer, len);
  }

  void setInterface(const std::string& local_network_interface) {
    local_network_interface_ = local_network_interface;
  }

 private:
#ifndef WIN32
  template<typename SocketType>
  void bindToLocalInterfaceIfSpecified(SocketType& socket) {
    if (local_network_interface_.empty()) {
      return;
    }

    using ifaddrs_uniq_ptr = std::unique_ptr<ifaddrs, utils::ifaddrs_deleter>;
    const auto if_list_ptr = []() -> ifaddrs_uniq_ptr {
      ifaddrs *list = nullptr;
      [[maybe_unused]] const auto get_ifa_success = getifaddrs(&list) == 0;
      assert(get_ifa_success || !list);
      return ifaddrs_uniq_ptr{ list };
    }();
    if (!if_list_ptr) {
      return;
    }

    const auto advance_func = [](const ifaddrs *const p) { return p->ifa_next; };
    const auto predicate = [this](const ifaddrs *const item) {
      return item->ifa_addr && item->ifa_name && (item->ifa_addr->sa_family == AF_INET || item->ifa_addr->sa_family == AF_INET6)
          && item->ifa_name == local_network_interface_;
    };
    auto item_found = [&]() -> ifaddrs* {
      for (auto it = if_list_ptr.get(); it; it = advance_func(it)) {
        if (predicate(it)) { return it; }
      }
      return nullptr;
    }();

    if (item_found == nullptr) {
      logger_->log_error("Could not find specified network interface: '{}'", local_network_interface_);
      return;
    }

    std::string address;
    try {
      address = utils::net::sockaddr_ntop(item_found->ifa_addr);
    } catch(const std::exception& ex) {
      logger_->log_error("Error occurred while getting network interface address: '{}'", ex.what());
      return;
    }

    asio::ip::tcp::endpoint local_endpoint(asio::ip::address::from_string(address), 0);
    asio::error_code err;
    socket.open(local_endpoint.protocol(), err);
    if (err) {
      logger_->log_error("Failed to open socket on network interface '{}' with the following message: '{}'", local_network_interface_, err.message());
      return;
    }
    socket.bind(local_endpoint, err);
    if (err) {
      logger_->log_error("Failed to bind to network interface '{}' with the following message: '{}'", local_network_interface_, err.message());
      return;
    }
  }
#endif

  bool connectTcpSocketOverSsl();
  bool connectTcpSocket();

  asio::io_context io_context_;
  std::unique_ptr<io::BaseStream> stream_;
  SocketData socket_data_;
  std::string local_network_interface_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AsioSocketConnection>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::utils::net

template<>
struct std::hash<org::apache::nifi::minifi::utils::net::ConnectionId> {
  size_t operator()(const org::apache::nifi::minifi::utils::net::ConnectionId& connection_id) const noexcept {
    return org::apache::nifi::minifi::utils::hash_combine(
        std::hash<std::string_view>{}(connection_id.getHostname()),
        std::hash<std::string_view>{}(connection_id.getService()));
  }
};

template <typename InternetProtocol>
struct fmt::formatter<asio::ip::basic_endpoint<InternetProtocol>> : fmt::ostream_formatter {};

template <>
struct fmt::formatter<asio::ip::address> : fmt::ostream_formatter {};
