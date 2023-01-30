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

#include <utility>
#include <memory>

#include "Server.h"
#include "Ssl.h"

namespace org::apache::nifi::minifi::utils::net {

class TcpServer : public Server {
 public:
  TcpServer(std::optional<size_t> max_queue_size_, uint16_t port, std::shared_ptr<core::logging::Logger> logger, std::optional<SslServerOptions> ssl_data)
      : Server(max_queue_size_, port, std::move(logger)),
        ssl_data_(std::move(ssl_data)) {
  }

 protected:
  asio::awaitable<void> doReceive() override;

  asio::awaitable<void> insecureSession(asio::ip::tcp::socket socket);
  asio::awaitable<void> secureSession(asio::ip::tcp::socket socket);

  asio::awaitable<void> readLoop(auto& socket);

  std::optional<SslServerOptions> ssl_data_;
};

}  // namespace org::apache::nifi::minifi::utils::net
