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

#include <string>
#include <utility>

#include "IpProtocol.h"
#include "asio/ts/internet.hpp"

namespace org::apache::nifi::minifi::utils::net {

struct Message {
  Message() = delete;
  Message(std::string message_data, IpProtocol protocol, asio::ip::address remote_address, asio::ip::port_type remote_port, asio::ip::port_type local_port)
      : message_data(std::move(message_data)),
      protocol(protocol),
      remote_address(std::move(remote_address)),
      remote_port(remote_port),
      local_port(local_port) {
  }

  bool is_partial = false;
  std::string message_data;
  IpProtocol protocol;
  asio::ip::address remote_address;
  asio::ip::port_type remote_port;
  asio::ip::port_type local_port;
};

}  // namespace org::apache::nifi::minifi::utils::net
