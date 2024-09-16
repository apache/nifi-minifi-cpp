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
#include <memory>

#include "utils/Enum.h"
#include "utils/MinifiConcurrentQueue.h"
#include "core/logging/Logger.h"
#include "asio/ts/buffer.hpp"
#include "asio/awaitable.hpp"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "Message.h"

namespace org::apache::nifi::minifi::utils::net {

class Server {
 public:
  virtual void run() {
    asio::co_spawn(io_context_, doReceive(), asio::detached);
    io_context_.run();
  }
  virtual void reset() {
    io_context_.restart();
  }
  virtual void stop() {
    io_context_.stop();
  }
  bool queueEmpty() {
    return concurrent_queue_.empty();
  }
  bool tryDequeue(utils::net::Message& received_message) {
    return concurrent_queue_.tryDequeue(received_message);
  }
  virtual ~Server() {
    stop();
  }

  uint16_t getPort() const {
    return port_;
  }

 protected:
  virtual asio::awaitable<void> doReceive() = 0;
  Server(std::optional<size_t> max_queue_size, uint16_t port, std::shared_ptr<core::logging::Logger> logger)
      : port_(port), max_queue_size_(max_queue_size), logger_(std::move(logger)) {}

  std::atomic<uint16_t> port_;
  utils::ConcurrentQueue<Message> concurrent_queue_;
  asio::io_context io_context_;
  std::optional<size_t> max_queue_size_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils::net
