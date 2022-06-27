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
#include <memory>
#include <string>

#include "Server.h"
#include "utils/MinifiConcurrentQueue.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "asio/ts/buffer.hpp"
#include "asio/ts/internet.hpp"
#include "asio/streambuf.hpp"

namespace org::apache::nifi::minifi::utils::net {

class UdpServer : public Server {
 public:
  UdpServer(std::optional<size_t> max_queue_size,
            uint16_t port,
            std::shared_ptr<core::logging::Logger> logger);

 private:
  void doReceive();

  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint sender_endpoint_;
  std::string buffer_;

  static constexpr size_t MAX_UDP_PACKET_SIZE = 65535;
};

}  // namespace org::apache::nifi::minifi::utils::net
