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
#include <tuple>
#include <memory>

#include "asio/ssl.hpp"
#include "asio/ip/tcp.hpp"

#include "AsioCoro.h"
#include "utils/Hash.h"
#include "utils/StringUtils.h"  // for string <=> on libc++
#include "controllers/SSLContextService.h"
#include "io/BaseStream.h"

namespace org::apache::nifi::minifi::utils::net {

using HandshakeType = asio::ssl::stream_base::handshake_type;
using TcpSocket = asio::ip::tcp::socket;
using SslSocket = asio::ssl::stream<asio::ip::tcp::socket>;

constexpr auto MINIFI_SSL_OPTIONS = asio::ssl::context::default_workarounds | asio::ssl::context::single_dh_use;

class ConnectionId {
 public:
  ConnectionId(std::string hostname, std::string port) : hostname_(std::move(hostname)), service_(std::move(port)) {}
  ConnectionId(const ConnectionId& connection_id) = default;
  ConnectionId(ConnectionId&& connection_id) = default;

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


asio::ssl::context getSslContext(const controllers::SSLContextService& ssl_context_service, asio::ssl::context::method ssl_context_method = asio::ssl::context::tlsv12_client);

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
  size_t write(const uint8_t *value, size_t len) override {
    gsl_Expects(stream_);
    return stream_->write(value, len);
  }

  void setInterface(const std::string& local_network_interface) {
    local_network_interface_ = local_network_interface;
  }

 private:
  template<typename SocketType>
  bool bindToLocalInterface(SocketType& socket) {
    if (local_network_interface_.empty()) {
      return true;
    }

    asio::ip::tcp::endpoint local_endpoint(asio::ip::address::from_string(local_network_interface_), 0);
    asio::error_code err;
    socket.open(local_endpoint.protocol(), err);
    if (err) {
      logger_->log_error("Failed to open socket on network interface '%s' with the following message: '%s'", local_network_interface_, err.message());
      return false;
    }
    socket.set_option(asio::ip::tcp::socket::reuse_address(true));
    socket.bind(local_endpoint, err);
    if (err) {
      logger_->log_error("Failed to bind to network interface '%s' with the following message: '%s'", local_network_interface_, err.message());
      return false;
    }

    return true;
  }

  bool connectTcpSocketOverSsl();
  bool connectTcpSocket();

  asio::io_context io_context_;
  std::unique_ptr<io::BaseStream> stream_;
  SocketData socket_data_;
  std::string local_network_interface_;
  std::shared_ptr<core::logging::Logger> logger_{core::logging::LoggerFactory<AsioSocketConnection>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::utils::net

namespace std {
template<>
struct hash<org::apache::nifi::minifi::utils::net::ConnectionId> {
  size_t operator()(const org::apache::nifi::minifi::utils::net::ConnectionId& connection_id) const {
    return org::apache::nifi::minifi::utils::hash_combine(
        std::hash<std::string_view>{}(connection_id.getHostname()),
        std::hash<std::string_view>{}(connection_id.getService()));
  }
};
}  // namespace std
