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
#include "utils/net/UdpServer.h"
#include "asio/use_awaitable.hpp"
#include "asio/detached.hpp"
#include "utils/net/AsioCoro.h"

namespace org::apache::nifi::minifi::utils::net {

constexpr size_t MAX_UDP_PACKET_SIZE = 65535;

UdpServer::UdpServer(std::optional<size_t> max_queue_size,
                     uint16_t port,
                     std::shared_ptr<core::logging::Logger> logger)
    : Server(max_queue_size, port, std::move(logger)) {
}

asio::awaitable<void> UdpServer::doReceive() {
  asio::ip::udp::socket socket(io_context_, asio::ip::udp::endpoint(asio::ip::udp::v6(), port_));
  if (port_ == 0)
    port_ = socket.local_endpoint().port();
  while (true) {
    std::string buffer = std::string(MAX_UDP_PACKET_SIZE, {});
    asio::ip::udp::endpoint sender_endpoint;

    auto [receive_error, bytes_received] = co_await socket.async_receive_from(asio::buffer(buffer, MAX_UDP_PACKET_SIZE), sender_endpoint, utils::net::use_nothrow_awaitable);
    if (receive_error) {
      logger_->log_warn("Error during receive: {}", receive_error.message());
      continue;
    }
    buffer.resize(bytes_received);
    if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size())
      concurrent_queue_.enqueue(utils::net::Message(std::move(buffer), IpProtocol::UDP, sender_endpoint.address(), socket.local_endpoint().port()));
    else
      logger_->log_warn("Queue is full. UDP message ignored.");
  }
}

}  // namespace org::apache::nifi::minifi::utils::net
