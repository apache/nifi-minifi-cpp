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

#include "SessionHandlingServer.h"

#include <memory>
#include <string>

#include "Ssl.h"
#include "asio/ssl.hpp"

namespace org::apache::nifi::minifi::utils::net {

using ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>;

class SslSession : public std::enable_shared_from_this<SslSession> {
 public:
  SslSession(asio::io_context& io_context, asio::ssl::context& context, utils::ConcurrentQueue<Message>& concurrent_queue,
    std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger);

  ssl_socket::lowest_layer_type& getSocket();
  void start();
  void handleReadUntilNewLine(std::error_code error_code);

 protected:
  utils::ConcurrentQueue<Message>& concurrent_queue_;
  std::optional<size_t> max_queue_size_;
  asio::basic_streambuf<std::allocator<char>> buffer_;
  std::shared_ptr<core::logging::Logger> logger_;
  ssl_socket socket_;
};

class SslServer : public SessionHandlingServer<SslSession> {
 public:
  SMART_ENUM(ClientAuthOption,
    (NONE, "NONE"),
    (WANT, "WANT"),
    (REQUIRED, "REQUIRED")
  )

  SslServer(std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger, SslData ssl_data, ClientAuthOption client_auth);

 protected:
  std::shared_ptr<SslSession> createSession() override;

  asio::ssl::context context_;
  SslData ssl_data_;
};

}  // namespace org::apache::nifi::minifi::utils::net
