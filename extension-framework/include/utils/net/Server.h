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

#include <list>
#include <optional>
#include <string>
#include <utility>
#include <memory>

#include "utils/Enum.h"
#include "utils/MinifiConcurrentQueue.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "asio/ts/buffer.hpp"
#include "asio/awaitable.hpp"
#include "asio/bind_cancellation_slot.hpp"
#include "asio/cancellation_signal.hpp"
#include "asio/co_spawn.hpp"
#include "asio/detached.hpp"
#include "asio/post.hpp"
#include "Message.h"

namespace org::apache::nifi::minifi::utils::net {

class Server {
 public:
  virtual void run() {
    asyncSpawn(doReceive());
    io_context_.run();
  }
  virtual void reset() {
    io_context_.restart();
  }
  virtual void stop() {
    asio::post(io_context_, [this] {
      for (auto& cancellation_signal : cancellation_signals_) {
        cancellation_signal.emit(asio::cancellation_type::all);
      }
    });
  }
  bool queueEmpty() {
    return concurrent_queue_.empty();
  }
  std::optional<utils::net::Message> tryDequeue() {
    return concurrent_queue_.tryDequeue();
  }
  Server(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) = delete;
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

  // Spawn a coroutine on io_context_ with a cancellation slot so stop() can end it and let the context drain
  // gracefully. Must be called from the io_context thread (i.e. from run() before io_context_.run(), or from within
  // a coroutine running on it); cancellation_signals_ is only ever touched on that thread, so it needs no locking.
  template<typename T>
  void asyncSpawn(asio::awaitable<T> coroutine) {
    const auto cancellation_signal_it = cancellation_signals_.emplace(cancellation_signals_.end());
    asio::co_spawn(io_context_, std::move(coroutine),
        asio::bind_cancellation_slot(cancellation_signal_it->slot(),
            [this, cancellation_signal_it](std::exception_ptr, auto&&...) { cancellation_signals_.erase(cancellation_signal_it); }));
  }

  std::atomic<uint16_t> port_;
  utils::ConcurrentQueue<Message> concurrent_queue_;
  asio::io_context io_context_;
  std::list<asio::cancellation_signal> cancellation_signals_;
  std::optional<size_t> max_queue_size_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::utils::net
