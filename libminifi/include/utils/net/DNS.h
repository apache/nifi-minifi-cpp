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

#include <chrono>
#include <string>
#include <string_view>
#include <system_error>

#include "nonstd/expected.hpp"
#include "asio/ip/address.hpp"

namespace org::apache::nifi::minifi::utils::net {

nonstd::expected<asio::ip::address, std::error_code> addressFromString(std::string_view ip_address_str);

nonstd::expected<std::string, std::error_code> reverseDnsLookup(const asio::ip::address& ip_address, std::chrono::steady_clock::duration timeout = std::chrono::seconds(5));

std::string getMyHostName();

}  // namespace org::apache::nifi::minifi::utils::net
