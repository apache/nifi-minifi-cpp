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
#include "utils/net/TcpServer.h"
#include "utils/net/AsioCoro.h"
#include "utils/net/AsioSocketUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::utils::net {

asio::awaitable<void> TcpServer::doReceive() {
  asio::ip::tcp::acceptor acceptor(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port_));
  if (port_ == 0)
    port_ = acceptor.local_endpoint().port();
  while (true) {
    auto [accept_error, socket] = co_await acceptor.async_accept(use_nothrow_awaitable);
    if (accept_error) {
      logger_->log_error("Error during accepting new connection: {}", accept_error.message());
      co_await utils::net::async_wait(1s);
      continue;
    }
    std::error_code error;
    auto remote_address = socket.lowest_layer().remote_endpoint(error).address();
    if (error)
      logger_->log_warn("Error during fetching remote endpoint: {}", error.message());
    auto local_port = socket.lowest_layer().local_endpoint(error).port();
    if (error)
      logger_->log_warn("Error during fetching local endpoint: {}", error.message());
    if (ssl_data_)
      co_spawn(io_context_, secureSession(std::move(socket), std::move(remote_address), local_port), asio::detached);
    else
      co_spawn(io_context_, insecureSession(std::move(socket), std::move(remote_address), local_port), asio::detached);
  }
}

asio::awaitable<void> TcpServer::readLoop(auto& socket, const auto& remote_address, const auto& local_port) {
  std::string read_message;
  while (true) {
    auto [read_error, bytes_read] = co_await asio::async_read_until(socket, asio::dynamic_buffer(read_message), delimiter_, use_nothrow_awaitable);  // NOLINT
    if (read_error) {
      if (read_error != asio::error::eof) {
        logger_->log_error("Error during reading from socket: {}", read_error.message());
      }
      co_return;
    }

    if (bytes_read == 0) {
      logger_->log_debug("No more bytes were read from socket");
      co_return;
    }

    if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size()) {
      auto message_str = read_message.substr(0, bytes_read - (consume_delimiter_ ? delimiter_.size() : 0));
      concurrent_queue_.enqueue(Message(std::move(message_str), IpProtocol::TCP, remote_address, local_port));
    } else {
      logger_->log_warn("Queue is full. TCP message ignored.");
    }
    read_message.erase(0, bytes_read);
  }
}

asio::awaitable<void> TcpServer::insecureSession(asio::ip::tcp::socket socket, asio::ip::address remote_address, asio::ip::port_type local_port) {
  co_return co_await readLoop(socket, remote_address, local_port);  // NOLINT
}

namespace {
asio::ssl::context setupSslContext(SslServerOptions& ssl_data) {
  asio::ssl::context ssl_context(asio::ssl::context::tls_server);
  ssl_context.set_options(minifi::utils::net::MINIFI_SSL_OPTIONS);
  ssl_context.set_password_callback([key_pw = ssl_data.cert_data.key_pw](std::size_t&, asio::ssl::context_base::password_purpose&) { return key_pw; });
  ssl_context.use_certificate_file(ssl_data.cert_data.cert_loc.string(), asio::ssl::context::pem);
  ssl_context.use_private_key_file(ssl_data.cert_data.key_loc.string(), asio::ssl::context::pem);
  if (!ssl_data.cert_data.ca_loc.empty())
    ssl_context.load_verify_file(ssl_data.cert_data.ca_loc.string());
  if (ssl_data.client_auth_option == ClientAuthOption::REQUIRED) {
    ssl_context.set_verify_mode(asio::ssl::verify_peer|asio::ssl::verify_fail_if_no_peer_cert);
  } else if (ssl_data.client_auth_option == ClientAuthOption::WANT) {
    ssl_context.set_verify_mode(asio::ssl::verify_peer);
  }
  return ssl_context;
}
}  // namespace

asio::awaitable<void> TcpServer::secureSession(asio::ip::tcp::socket socket, asio::ip::address remote_address, asio::ip::port_type local_port) {
  gsl_Expects(ssl_data_);
  auto ssl_context = setupSslContext(*ssl_data_);
  SslSocket ssl_socket(std::move(socket), ssl_context);
  auto [handshake_error] = co_await ssl_socket.async_handshake(HandshakeType::server, use_nothrow_awaitable);
  if (handshake_error) {
    logger_->log_warn("Handshake with {} failed due to {}", remote_address, handshake_error.message());
    co_return;
  }
  co_await readLoop(ssl_socket, remote_address, local_port);  // NOLINT

  asio::error_code ec;
  ssl_socket.lowest_layer().cancel(ec);
  if (ec) {
    logger_->log_error("Cancelling asynchronous operations of SSL socket failed with: {}", ec.message());
  }
  auto [shutdown_error] = co_await ssl_socket.async_shutdown(use_nothrow_awaitable);
  if (shutdown_error) {
    logger_->log_warn("Shutdown of {} failed with {}", remote_address, shutdown_error.message());
  }
}

}  // namespace org::apache::nifi::minifi::utils::net
