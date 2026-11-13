// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <system_error>
#include <expected>

#include "asio/ip/address.hpp"

namespace org::apache::nifi::minifi::utils::net {

std::expected<asio::ip::address, std::error_code> addressFromString(std::string_view ip_address_str);

std::expected<std::string, std::error_code> reverseDnsLookup(const asio::ip::address& ip_address, std::chrono::steady_clock::duration timeout = std::chrono::seconds(5));

std::string getMyHostName();

}  // namespace org::apache::nifi::minifi::utils::net
