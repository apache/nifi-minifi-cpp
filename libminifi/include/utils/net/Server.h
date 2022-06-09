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

#include <optional>
#include <string>
#include <utility>

#include "utils/Enum.h"
#include "utils/MinifiConcurrentQueue.h"
#include "core/logging/Logger.h"
#include "asio/ts/buffer.hpp"
#include "asio/ts/internet.hpp"
#include "asio/streambuf.hpp"

namespace org::apache::nifi::minifi::utils::net {

SMART_ENUM(Protocol,
  (TCP, "TCP"),
  (UDP, "UDP")
)

struct Message {
 public:
  Message() = default;
  Message(std::string message_data, Protocol protocol, asio::ip::address sender_address, asio::ip::port_type server_port)
    : message_data(std::move(message_data)),
      protocol(protocol),
      server_port(server_port),
      sender_address(std::move(sender_address)) {
  }

  std::string message_data;
  Protocol protocol;
  asio::ip::port_type server_port;
  asio::ip::address sender_address;
};

class Server {
 public:
  virtual ~Server() = default;

 protected:
  Server(asio::io_context& io_context, utils::ConcurrentQueue<Message>& concurrent_queue, std::optional<size_t> max_queue_size, std::shared_ptr<core::logging::Logger> logger)
      : concurrent_queue_(concurrent_queue), io_context_(io_context), max_queue_size_(max_queue_size), logger_(logger) {}

  utils::ConcurrentQueue<Message>& concurrent_queue_;
  asio::io_context& io_context_;
  std::optional<size_t> max_queue_size_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils::net
