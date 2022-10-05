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

namespace org::apache::nifi::minifi::utils::net {

UdpServer::UdpServer(std::optional<size_t> max_queue_size,
                     uint16_t port,
                     std::shared_ptr<core::logging::Logger> logger)
    : Server(max_queue_size, std::move(logger)),
      socket_(io_context_, asio::ip::udp::endpoint(asio::ip::udp::v6(), port)) {
  doReceive();
}


void UdpServer::doReceive() {
  buffer_.resize(MAX_UDP_PACKET_SIZE);
  socket_.async_receive_from(asio::buffer(buffer_, MAX_UDP_PACKET_SIZE),
                             sender_endpoint_,
                             [this](std::error_code ec, std::size_t bytes_received) {
                               if (!ec && bytes_received > 0) {
                                 buffer_.resize(bytes_received);
                                 if (!max_queue_size_ || max_queue_size_ > concurrent_queue_.size())
                                   concurrent_queue_.enqueue(utils::net::Message(std::move(buffer_), IpProtocol::UDP, sender_endpoint_.address(), socket_.local_endpoint().port()));
                                 else
                                   logger_->log_warn("Queue is full. UDP message ignored.");
                               }
                               doReceive();
                             });
}

}  // namespace org::apache::nifi::minifi::utils::net
