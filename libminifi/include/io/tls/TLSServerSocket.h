/**
 *
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

#include <memory>
#include <string>
#include <vector>

#include "TLSSocket.h"
#include "../ServerSocket.h"

namespace org::apache::nifi::minifi::io {

/**
 * Purpose: Server socket abstraction that makes focusing the accept/block paradigm
 * simpler.
 */
class TLSServerSocket : public BaseServerSocket, public TLSSocket {
 public:
  explicit TLSServerSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners);

  virtual ~TLSServerSocket();

  int16_t initialize(bool loopbackOnly) override {
    is_loopback_only_ = loopbackOnly;
    return TLSSocket::initialize();
  }

  int initialize() override {
    return TLSSocket::initialize();
  }

  /**
   * Registers a call back and starts the read for the server socket.
   */
  void registerCallback(
      std::function<bool()> accept_function,
      std::function<size_t(std::vector<uint8_t>*)> handler,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

  /**
   * Initializes the socket
   * @return result of the creation operation.
   */
  void registerCallback(std::function<bool()> accept_function, std::function<void(io::BaseStream *)> handler) override;

 private:
  void close_fd(int fd);

  std::atomic<bool> running_;

  std::thread server_read_thread_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<TLSServerSocket>::getLogger();
};

}  // namespace org::apache::nifi::minifi::io
