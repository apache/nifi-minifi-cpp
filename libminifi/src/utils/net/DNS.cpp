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

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include "utils/net/Socket.h"
#else
#include <netdb.h>
#include <cstring>
#endif /* WIN32 */

namespace org::apache::nifi::minifi::utils::net {

namespace {

#ifndef WIN32
class addrinfo_category : public std::error_category {
 public:
  [[nodiscard]] const char* name() const noexcept override { return "addrinfo"; }

  [[nodiscard]] std::string message(int value) const override {
    return gai_strerror(value);
  }
};

const addrinfo_category& get_addrinfo_category() {
  static addrinfo_category instance;
  return instance;
}
#endif

std::error_code get_last_getaddrinfo_err_code(int getaddrinfo_result) {
#ifdef WIN32
  (void)getaddrinfo_result;  // against unused warnings on windows
  return std::error_code{WSAGetLastError(), std::system_category()};
#else
  return std::error_code{getaddrinfo_result, get_addrinfo_category()};
#endif /* WIN32 */
}
}  // namespace

void addrinfo_deleter::operator()(addrinfo* const p) const noexcept {
  freeaddrinfo(p);
}

nonstd::expected<gsl::not_null<std::unique_ptr<addrinfo, addrinfo_deleter>>, std::error_code> resolveHost(const char* const hostname, const char* const port, const IpProtocol protocol,
    const bool need_canonname) {
  addrinfo hints{};
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = protocol == IpProtocol::TCP ? SOCK_STREAM : SOCK_DGRAM;
  hints.ai_flags = need_canonname ? AI_CANONNAME : 0;
  if (!hostname)
    hints.ai_flags |= AI_PASSIVE;
  hints.ai_protocol = [protocol]() -> int {
    switch (protocol.value()) {
      case IpProtocol::TCP: return IPPROTO_TCP;
      case IpProtocol::UDP: return IPPROTO_UDP;
    }
    return 0;
  }();

  addrinfo* getaddrinfo_result = nullptr;
  const int errcode = getaddrinfo(hostname, port, &hints, &getaddrinfo_result);
  auto addr_info = gsl::make_not_null(std::unique_ptr<addrinfo, addrinfo_deleter>{getaddrinfo_result});
  getaddrinfo_result = nullptr;
  if (errcode != 0) {
    return nonstd::make_unexpected(get_last_getaddrinfo_err_code(errcode));
  }
  return addr_info;
}

}  // namespace org::apache::nifi::minifi::utils::net
