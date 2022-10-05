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
#include "asio/ssl.hpp"

namespace org::apache::nifi::minifi::utils::net {

template<typename SessionType>
class SessionHandlingServer : public Server {
 public:
  SessionHandlingServer(std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger)
      : Server(max_queue_size, std::move(logger)),
        acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port)) {
  }

  void run() override {
    startAccept();
    Server::run();
  }

 protected:
  void startAccept() {
    auto new_session = createSession();
    acceptor_.async_accept(new_session->getSocket(),
                           [this, new_session](const auto& error_code) -> void {
                             handleAccept(new_session, error_code);
                           });
  }

  void handleAccept(const std::shared_ptr<SessionType>& session, const std::error_code& error) {
    if (error) {
      return;
    }

    session->start();
    auto new_session = createSession();
    acceptor_.async_accept(new_session->getSocket(),
                           [this, new_session](const auto& error_code) -> void {
                             handleAccept(new_session, error_code);
                           });
  }

  virtual std::shared_ptr<SessionType> createSession() = 0;

  asio::ip::tcp::acceptor acceptor_;
};

}  // namespace org::apache::nifi::minifi::utils::net
