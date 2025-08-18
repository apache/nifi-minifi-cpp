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
#include "utils/net/DNS.h"

#include "Exception.h"
#include "utils/StringUtils.h"
#include "utils/net/AsioCoro.h"
#include "asio/detached.hpp"
#include "asio/ip/udp.hpp"
#include "asio/ip/host_name.hpp"

namespace org::apache::nifi::minifi::utils::net {

nonstd::expected<asio::ip::address, std::error_code> addressFromString(const std::string_view ip_address_str) {
  std::error_code ip_address_from_string_error;
  auto ip_address = asio::ip::address::from_string(ip_address_str.data(), ip_address_from_string_error);
  if (ip_address_from_string_error)
    return nonstd::make_unexpected(ip_address_from_string_error);
  return ip_address;
}

namespace {
asio::awaitable<std::tuple<std::error_code, asio::ip::basic_resolver<asio::ip::udp>::results_type>> asyncReverseDnsLookup(const asio::ip::address& ip_address,
    std::chrono::steady_clock::duration timeout_duration) {
  asio::ip::basic_resolver<asio::ip::udp> resolver(co_await asio::this_coro::executor);
  co_return co_await asyncOperationWithTimeout(resolver.async_resolve({ip_address, 0}, use_nothrow_awaitable), timeout_duration);
}
}  // namespace

nonstd::expected<std::string, std::error_code> reverseDnsLookup(const asio::ip::address& ip_address, std::chrono::steady_clock::duration timeout_duration) {
  asio::io_context io_context;

  std::error_code resolve_error;
  asio::ip::basic_resolver<asio::ip::udp>::results_type results;

  co_spawn(io_context, asyncReverseDnsLookup(ip_address, timeout_duration), [&resolve_error, &results](const std::exception_ptr&, const auto& resolve_results) {
    resolve_error = std::get<std::error_code>(resolve_results);
    results = std::get<asio::ip::basic_resolver<asio::ip::udp>::results_type>(resolve_results);
  });
  io_context.run();

  if (resolve_error)
    return nonstd::make_unexpected(resolve_error);
  return results->host_name();
}

std::string getMyHostName() {
  return asio::ip::host_name();
}

}  // namespace org::apache::nifi::minifi::utils::net
