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

namespace org::apache::nifi::minifi::utils::net {

TcpSession::TcpSession(asio::io_context& io_context, utils::ConcurrentQueue<Message>& concurrent_queue, std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger)
  : concurrent_queue_(concurrent_queue),
    max_queue_size_(max_queue_size),
    socket_(io_context),
    logger_(std::move(logger)) {
}

asio::ip::tcp::socket& TcpSession::getSocket() {
  return socket_;
}

void TcpSession::start() {
  asio::async_read_until(socket_,
                         buffer_,
                         '\n',
                         [self = shared_from_this()](const auto& error_code, size_t) -> void {
                           self->handleReadUntilNewLine(error_code);
                         });
}

void TcpSession::handleReadUntilNewLine(std::error_code error_code) {
  if (error_code)
    return;
  std::istream is(&buffer_);
  std::string message;
  std::getline(is, message);
  if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size())
    concurrent_queue_.enqueue(Message(message, Protocol::TCP, socket_.remote_endpoint().address(), socket_.local_endpoint().port()));
  else
    logger_->log_warn("Queue is full. Syslog message ignored.");
  asio::async_read_until(socket_,
                         buffer_,
                         '\n',
                         [self = shared_from_this()](const auto& error_code, size_t) -> void {
                           self->handleReadUntilNewLine(error_code);
                         });
}

TcpServer::TcpServer(
  asio::io_context& io_context, utils::ConcurrentQueue<Message>& concurrent_queue, std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger)
    : Server(io_context, concurrent_queue, max_queue_size, std::move(logger)),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
  startAccept();
}

void TcpServer::startAccept() {
  auto new_session = std::make_shared<TcpSession>(io_context_, concurrent_queue_, max_queue_size_, logger_);
  acceptor_.async_accept(new_session->getSocket(),
                         [this, new_session](const auto& error_code) -> void {
                           handleAccept(new_session, error_code);
                         });
}

void TcpServer::handleAccept(const std::shared_ptr<TcpSession>& session, const std::error_code& error) {
  if (error)
    return;

  session->start();
  auto new_session = std::make_shared<TcpSession>(io_context_, concurrent_queue_, max_queue_size_, logger_);
  acceptor_.async_accept(new_session->getSocket(),
                         [this, new_session](const auto& error_code) -> void {
                           handleAccept(new_session, error_code);
                         });
}

}  // namespace org::apache::nifi::minifi::utils::net
