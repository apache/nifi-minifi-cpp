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

#include "utils/net/AsioSocketUtils.h"
#include "controllers/SSLContextService.h"
#include "io/AsioStream.h"

#include "asio/connect.hpp"

namespace org::apache::nifi::minifi::utils::net {

template<>
asio::awaitable<std::tuple<std::error_code>> handshake(TcpSocket&, asio::steady_timer::duration) {
  co_return std::error_code();
}

template<>
asio::awaitable<std::tuple<std::error_code>> handshake(SslSocket& socket, asio::steady_timer::duration timeout_duration) {
  co_return co_await asyncOperationWithTimeout(socket.async_handshake(HandshakeType::client, use_nothrow_awaitable), timeout_duration);  // NOLINT
}

asio::ssl::context getSslContext(const controllers::SSLContextService& ssl_context_service, asio::ssl::context::method ssl_context_method) {
  asio::ssl::context ssl_context(ssl_context_method);
  ssl_context.set_options(MINIFI_SSL_OPTIONS);
  if (const auto& ca_cert = ssl_context_service.getCACertificate(); !ca_cert.empty())
    ssl_context.load_verify_file(ssl_context_service.getCACertificate().string());
  ssl_context.set_verify_mode(asio::ssl::verify_peer);
  ssl_context.set_password_callback([password = ssl_context_service.getPassphrase()](std::size_t&, asio::ssl::context_base::password_purpose&) { return password; });
  if (const auto& cert_file = ssl_context_service.getCertificateFile(); !cert_file.empty())
    ssl_context.use_certificate_file(cert_file.string(), asio::ssl::context::pem);
  if (const auto& private_key_file = ssl_context_service.getPrivateKeyFile(); !private_key_file.empty())
    ssl_context.use_private_key_file(private_key_file.string(), asio::ssl::context::pem);
  return ssl_context;
}

AsioSocketConnection::AsioSocketConnection(SocketData socket_data) : socket_data_(std::move(socket_data)) {
}

int AsioSocketConnection::initialize() {
  bool result = false;
  if (socket_data_.ssl_context_service) {
    result = connectTcpSocketOverSsl();
  } else {
    result = connectTcpSocket();
  }
  return result ? 0 : -1;
}

bool AsioSocketConnection::connectTcpSocketOverSsl() {
  auto ssl_context = utils::net::getSslContext(*socket_data_.ssl_context_service);
  asio::ssl::stream<asio::ip::tcp::socket> socket(io_context_, ssl_context);

#ifndef WIN32
  bindToLocalInterfaceIfSpecified(socket.lowest_layer());
#endif

  asio::ip::tcp::resolver resolver(io_context_);
  asio::error_code err;
  asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(socket_data_.host, std::to_string(socket_data_.port), err);
  if (err) {
    logger_->log_error("Resolving host '{}' on port '{}' failed with the following message: '{}'", socket_data_.host, socket_data_.port, err.message());
    return false;
  }

  asio::connect(socket.lowest_layer(), endpoints, err);
  if (err) {
    logger_->log_error("Connecting to host '{}' on port '{}' failed with the following message: '{}'", socket_data_.host, socket_data_.port, err.message());
    return false;
  }
  socket.handshake(asio::ssl::stream_base::client, err);
  if (err) {
    logger_->log_error("SSL handshake failed while connecting to host '{}' on port '{}' with the following message: '{}'", socket_data_.host, socket_data_.port, err.message());
    return false;
  }
  stream_ = std::make_unique<io::AsioStream<asio::ssl::stream<asio::ip::tcp::socket>>>(std::move(socket));
  return true;
}

bool AsioSocketConnection::connectTcpSocket() {
  asio::ip::tcp::socket socket(io_context_);

#ifndef WIN32
  bindToLocalInterfaceIfSpecified(socket);
#endif

  asio::ip::tcp::resolver resolver(io_context_);
  asio::error_code err;
  asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(socket_data_.host, std::to_string(socket_data_.port));
  if (err) {
    logger_->log_error("Resolving host '{}' on port '{}' failed with the following message: '{}'", socket_data_.host, socket_data_.port, err.message());
    return false;
  }

  asio::connect(socket, endpoints, err);
  if (err) {
    logger_->log_error("Connecting to host '{}' on port '{}' failed with the following message: '{}'", socket_data_.host, socket_data_.port, err.message());
    return false;
  }
  stream_ = std::make_unique<io::AsioStream<asio::ip::tcp::socket>>(std::move(socket));
  return true;
}
}  // namespace org::apache::nifi::minifi::utils::net
