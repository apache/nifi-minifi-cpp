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

namespace org::apache::nifi::minifi::utils::net {

asio::awaitable<void> TcpServer::doReceive() {
  asio::ip::tcp::acceptor acceptor(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port_));
  if (port_ == 0)
    port_ = acceptor.local_endpoint().port();
  while (true) {
    auto [accept_error, socket] = co_await acceptor.async_accept(use_nothrow_awaitable);
    if (accept_error) {
      logger_->log_error("Error during accepting new connection: %s", accept_error.message());
      break;
    }
    if (ssl_data_)
      co_spawn(io_context_, secureSession(std::move(socket)), asio::detached);
    else
      co_spawn(io_context_, insecureSession(std::move(socket)), asio::detached);
  }
}

asio::awaitable<void> TcpServer::readLoop(auto& socket) {
  std::string read_message;
  while (true) {
    auto [read_error, bytes_read] = co_await asio::async_read_until(socket, asio::dynamic_buffer(read_message), '\n', use_nothrow_awaitable);  // NOLINT
    if (read_error || bytes_read == 0)
      co_return;

    if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size())
      concurrent_queue_.enqueue(Message(read_message.substr(0, bytes_read - 1), IpProtocol::TCP, socket.lowest_layer().remote_endpoint().address(), socket.lowest_layer().local_endpoint().port()));
    else
      logger_->log_warn("Queue is full. TCP message ignored.");
    read_message.erase(0, bytes_read);
  }
}

asio::awaitable<void> TcpServer::insecureSession(asio::ip::tcp::socket socket) {
  co_return co_await readLoop(socket);  // NOLINT
}

namespace {
asio::ssl::context setupSslContext(SslServerOptions& ssl_data) {
  asio::ssl::context ssl_context(asio::ssl::context::tlsv12_server);
  ssl_context.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::single_dh_use | asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1);
  ssl_context.set_password_callback([key_pw = ssl_data.cert_data.key_pw](std::size_t&, asio::ssl::context_base::password_purpose&) { return key_pw; });
  ssl_context.use_certificate_file(ssl_data.cert_data.cert_loc.string(), asio::ssl::context::pem);
  ssl_context.use_private_key_file(ssl_data.cert_data.key_loc.string(), asio::ssl::context::pem);
  ssl_context.load_verify_file(ssl_data.cert_data.ca_loc.string());
  if (ssl_data.client_auth_option == ClientAuthOption::REQUIRED) {
    ssl_context.set_verify_mode(asio::ssl::verify_peer|asio::ssl::verify_fail_if_no_peer_cert);
  } else if (ssl_data.client_auth_option == ClientAuthOption::WANT) {
    ssl_context.set_verify_mode(asio::ssl::verify_peer);
  }
  return ssl_context;
}
}  // namespace

asio::awaitable<void> TcpServer::secureSession(asio::ip::tcp::socket socket) {
  gsl_Expects(ssl_data_);
  auto ssl_context = setupSslContext(*ssl_data_);
  SslSocket ssl_socket(std::move(socket), ssl_context);
  auto [handshake_error] = co_await ssl_socket.async_handshake(HandshakeType::server, use_nothrow_awaitable);
  if (handshake_error) {
    core::logging::LOG_WARN(logger_) << "Handshake with " << ssl_socket.lowest_layer().remote_endpoint() << " failed due to " << handshake_error.message();
    co_return;
  }
  co_return co_await readLoop(ssl_socket);  // NOLINT
}

}  // namespace org::apache::nifi::minifi::utils::net
