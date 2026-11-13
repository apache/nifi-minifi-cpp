// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
