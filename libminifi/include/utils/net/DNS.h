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
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include "nonstd/expected.hpp"
#include "utils/gsl.h"
#include "IpProtocol.h"

struct addrinfo;

namespace org::apache::nifi::minifi::utils::net {
struct addrinfo_deleter {
  void operator()(addrinfo*) const noexcept;
};

nonstd::expected<gsl::not_null<std::unique_ptr<addrinfo, addrinfo_deleter>>, std::error_code> resolveHost(const char* hostname, const char* port, IpProtocol = IpProtocol::TCP,
    bool need_canonname = false);
inline auto resolveHost(const char* const port, const IpProtocol proto = IpProtocol::TCP, const bool need_canonname = false) {
  return resolveHost(nullptr, port, proto, need_canonname);
}
inline auto resolveHost(const char* const hostname, const uint16_t port, const IpProtocol proto = IpProtocol::TCP, const bool need_canonname = false) {
  return resolveHost(hostname, std::to_string(port).c_str(), proto, need_canonname);
}
inline auto resolveHost(const uint16_t port, const IpProtocol proto = IpProtocol::TCP, const bool need_canonname = false) {
  return resolveHost(nullptr, port, proto, need_canonname);
}
}  // namespace org::apache::nifi::minifi::utils::net
