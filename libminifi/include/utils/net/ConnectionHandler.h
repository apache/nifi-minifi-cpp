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

#include <asio/read.hpp>

#include "AsioCoro.h"
#include "AsioSocketUtils.h"
#include "ConnectionHandlerBase.h"

namespace org::apache::nifi::minifi::utils::net {

template<class SocketType>
class ConnectionHandler final : public ConnectionHandlerBase {
 public:
  ConnectionHandler(ConnectionId connection_id,
                    const std::chrono::milliseconds timeout,
                    std::shared_ptr<core::logging::Logger> logger,
                    const std::optional<size_t> max_size_of_socket_send_buffer,
                    asio::ssl::context* ssl_context)
      : connection_id_(std::move(connection_id)),
        timeout_duration_(timeout),
        logger_(std::move(logger)),
        max_size_of_socket_send_buffer_(max_size_of_socket_send_buffer),
        ssl_context_(ssl_context) {
  }

  ConnectionHandler(ConnectionHandler&&) = delete;
  ConnectionHandler(const ConnectionHandler&) = delete;
  ConnectionHandler& operator=(ConnectionHandler&&) = delete;
  ConnectionHandler& operator=(const ConnectionHandler&) = delete;

  ~ConnectionHandler() override {
    shutdownSocket();
  }


 private:
  [[nodiscard]] bool hasBeenUsedIn(std::chrono::milliseconds dur) const override {
    return last_used_ && *last_used_ >= (std::chrono::steady_clock::now() - dur);
  }

  void reset() override {
    last_used_.reset();
    socket_.reset();
  }

  [[nodiscard]] bool hasBeenUsed() const override { return last_used_.has_value(); }
  [[nodiscard]] asio::awaitable<std::error_code> setupUsableSocket(asio::io_context& io_context) override;
  [[nodiscard]] bool hasUsableSocket() const { return socket_ && socket_->lowest_layer().is_open(); }

  asio::awaitable<std::error_code> establishNewConnection(const asio::ip::tcp::resolver::results_type& endpoints, asio::io_context& io_context_);
  [[nodiscard]] asio::awaitable<std::tuple<std::error_code, size_t>> write(const asio::const_buffer& buffer) override;
  [[nodiscard]] asio::awaitable<std::tuple<std::error_code, size_t>> read(asio::mutable_buffer& buffer) override;

  SocketType createNewSocket(asio::io_context& io_context_);
  void shutdownSocket();

  ConnectionId connection_id_;
  std::optional<SocketType> socket_{};

  std::optional<std::chrono::steady_clock::time_point> last_used_{};
  asio::steady_timer::duration timeout_duration_{};

  std::shared_ptr<core::logging::Logger> logger_{};
  std::optional<size_t> max_size_of_socket_send_buffer_{};

  asio::ssl::context* ssl_context_{};
};

template<>
inline TcpSocket ConnectionHandler<TcpSocket>::createNewSocket(asio::io_context& io_context_) {
  gsl_Expects(!ssl_context_);
  return TcpSocket{io_context_};
}

template<>
inline SslSocket ConnectionHandler<SslSocket>::createNewSocket(asio::io_context& io_context_) {
  gsl_Expects(ssl_context_);
  return {io_context_, *ssl_context_};
}

template<>
inline void ConnectionHandler<TcpSocket>::shutdownSocket() {
}

template<>
inline void ConnectionHandler<SslSocket>::shutdownSocket() {
  gsl_Expects(ssl_context_);
  if (socket_) {
    asio::error_code ec;
    socket_->lowest_layer().cancel(ec);
    if (ec) {
      logger_->log_error("Cancelling asynchronous operations of SSL socket failed with: {}", ec.message());
    }
    socket_->shutdown(ec);
    if (ec) {
      logger_->log_error("Shutdown of SSL socket failed with: {}", ec.message());
    }
  }
}

template<class SocketType>
asio::awaitable<std::error_code> ConnectionHandler<SocketType>::establishNewConnection(const asio::ip::tcp::resolver::results_type& endpoints, asio::io_context& io_context) {
  auto socket = createNewSocket(io_context);
  std::error_code last_error;
  for (const auto& endpoint : endpoints) {
    auto [connection_error] = co_await asyncOperationWithTimeout(socket.lowest_layer().async_connect(endpoint, use_nothrow_awaitable), timeout_duration_);
    if (connection_error) {
      logger_->log_debug("Connecting to {} failed due to {}", endpoint.endpoint(), connection_error.message());
      last_error = connection_error;
      continue;
    }
    auto [handshake_error] = co_await handshake(socket, timeout_duration_);
    if (handshake_error) {
      logger_->log_debug("Handshake with {} failed due to {}", endpoint.endpoint(), handshake_error.message());
      last_error = handshake_error;
      continue;
    }
    if (max_size_of_socket_send_buffer_)
      socket.lowest_layer().set_option(TcpSocket::send_buffer_size(gsl::narrow<int>(*max_size_of_socket_send_buffer_)));
    socket_.emplace(std::move(socket));
    co_return std::error_code();
  }
  co_return last_error;
}

template<class SocketType>
[[nodiscard]] asio::awaitable<std::error_code> ConnectionHandler<SocketType>::setupUsableSocket(asio::io_context& io_context) {
  if (hasUsableSocket())
    co_return std::error_code();
  asio::ip::tcp::resolver resolver(io_context);
  auto [resolve_error, resolve_result] = co_await asyncOperationWithTimeout(
      resolver.async_resolve(connection_id_.getHostname(), connection_id_.getService(), use_nothrow_awaitable), timeout_duration_);
  if (resolve_error)
    co_return resolve_error;
  co_return co_await establishNewConnection(resolve_result, io_context);
}

template<class SocketType>
asio::awaitable<std::tuple<std::error_code, size_t>> ConnectionHandler<SocketType>::write(const asio::const_buffer& buffer) {
  auto result = co_await asyncOperationWithTimeout(asio::async_write(*socket_, buffer, use_nothrow_awaitable), timeout_duration_);
  if (!std::get<std::error_code>(result)) {
    last_used_ = std::chrono::steady_clock::now();
  }
  co_return result;
}

template<class SocketType>
asio::awaitable<std::tuple<std::error_code, size_t>> ConnectionHandler<SocketType>::read(asio::mutable_buffer& buffer) {
  auto result = co_await asyncOperationWithTimeout(asio::async_read(*socket_, buffer, use_nothrow_awaitable), timeout_duration_);
  if (!std::get<std::error_code>(result)) {
    last_used_ = std::chrono::steady_clock::now();
  }
  co_return result;
}

}  // namespace org::apache::nifi::minifi::utils::net
