// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
