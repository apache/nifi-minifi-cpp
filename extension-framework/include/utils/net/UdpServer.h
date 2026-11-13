// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <optional>
#include <memory>
#include <asio/awaitable.hpp>

#include "Server.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::utils::net {

class UdpServer : public Server {
 public:
  UdpServer(std::optional<size_t> max_queue_size,
            uint16_t port,
            std::shared_ptr<core::logging::Logger> logger);

 private:
  asio::awaitable<void> doReceive() override;
};

}  // namespace org::apache::nifi::minifi::utils::net
