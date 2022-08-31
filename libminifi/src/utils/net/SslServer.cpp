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
#include "utils/net/SslServer.h"

namespace org::apache::nifi::minifi::utils::net {

SslSession::SslSession(asio::io_context& io_context, asio::ssl::context& context, utils::ConcurrentQueue<Message>& concurrent_queue,
    std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger)
  : concurrent_queue_(concurrent_queue),
    max_queue_size_(max_queue_size),
    logger_(std::move(logger)),
    socket_(io_context, context) {
}

ssl_socket::lowest_layer_type& SslSession::getSocket() {
  return socket_.lowest_layer();
}

void SslSession::start() {
  socket_.async_handshake(asio::ssl::stream_base::server,
    [this, self = shared_from_this()](const std::error_code& error_code) {
      if (error_code) {
        logger_->log_error("Error occured during SSL handshake: (%d) %s", error_code.value(), error_code.message());
        return;
      }
      asio::async_read_until(socket_,
                             buffer_,
                             '\n',
                             [self](const auto& error_code, size_t) -> void {
                               self->handleReadUntilNewLine(error_code);
                             });
    });
}

void SslSession::handleReadUntilNewLine(std::error_code error_code) {
  if (error_code)
    return;
  std::istream is(&buffer_);
  std::string message;
  std::getline(is, message);
  if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size())
    concurrent_queue_.enqueue(Message(message, IpProtocol::TCP, getSocket().remote_endpoint().address(), getSocket().local_endpoint().port()));
  else
    logger_->log_warn("Queue is full. TCP message ignored.");
  asio::async_read_until(socket_,
                         buffer_,
                         '\n',
                         [self = shared_from_this()](const auto& error_code, size_t) -> void {
                           self->handleReadUntilNewLine(error_code);
                         });
}

SslServer::SslServer(std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger, SslData ssl_data, ClientAuthOption client_auth)
    : SessionHandlingServer<SslSession>(max_queue_size, port, std::move(logger)),
      context_(asio::ssl::context::sslv23),
      ssl_data_(std::move(ssl_data)) {
    context_.set_options(
        asio::ssl::context::default_workarounds
        | asio::ssl::context::no_sslv2
        | asio::ssl::context::single_dh_use);
    context_.set_password_callback([this](std::size_t&, asio::ssl::context_base::password_purpose&) { return ssl_data_.key_pw; });
    context_.use_certificate_file(ssl_data_.cert_loc, asio::ssl::context::pem);
    context_.use_private_key_file(ssl_data_.key_loc, asio::ssl::context::pem);
    context_.load_verify_file(ssl_data_.ca_loc);
    if (client_auth == ClientAuthOption::REQUIRED) {
      context_.set_verify_mode(asio::ssl::verify_peer|asio::ssl::verify_fail_if_no_peer_cert);
    } else if (client_auth == ClientAuthOption::WANT) {
      context_.set_verify_mode(asio::ssl::verify_peer);
    }
}

std::shared_ptr<SslSession> SslServer::createSession() {
  return std::make_shared<SslSession>(io_context_, context_, concurrent_queue_, max_queue_size_, logger_);
}

}  // namespace org::apache::nifi::minifi::utils::net
