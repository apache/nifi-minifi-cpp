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

namespace org::apache::nifi::minifi::utils::net {

class ConnectionHandlerBase {
 public:
  ConnectionHandlerBase() = default;
  ConnectionHandlerBase(const ConnectionHandlerBase& connection_id) = delete;
  ConnectionHandlerBase(ConnectionHandlerBase&& connection_id) = delete;
  ConnectionHandlerBase& operator=(ConnectionHandlerBase&&) = delete;
  ConnectionHandlerBase& operator=(const ConnectionHandlerBase&) = delete;

  virtual ~ConnectionHandlerBase() = default;

  virtual void reset() = 0;

  [[nodiscard]] virtual asio::awaitable<std::error_code> setupUsableSocket(asio::io_context& io_context) = 0;
  [[nodiscard]] virtual bool hasBeenUsed() const = 0;
  [[nodiscard]] virtual bool hasBeenUsedIn(std::chrono::milliseconds dur) const = 0;
  [[nodiscard]] virtual asio::awaitable<std::tuple<std::error_code, size_t>> write(const asio::const_buffer& buffer) = 0;
  [[nodiscard]] virtual asio::awaitable<std::tuple<std::error_code, size_t>> read(asio::mutable_buffer& buffer) = 0;
};

}  // namespace org::apache::nifi::minifi::utils::net
