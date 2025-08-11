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
#include "utils/net/Socket.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include <cstring>
#include "Exception.h"

namespace org::apache::nifi::minifi::utils::net {

namespace {
std::error_code get_last_socket_error() {
#ifdef WIN32
  const auto error_code = WSAGetLastError();
#else
  const auto error_code = errno;
#endif /* WIN32 */
  return {error_code, std::system_category()};
}
}  // namespace

std::string sockaddr_ntop(const sockaddr* const sa) {
  std::string result;
  if (sa->sa_family == AF_INET) {
    sockaddr_in sa_in{};
    std::memcpy(&sa_in, sa, sizeof(sockaddr_in));
    result.resize(INET_ADDRSTRLEN);
    if (inet_ntop(AF_INET, &sa_in.sin_addr, result.data(), INET_ADDRSTRLEN) == nullptr) {
      throw minifi::Exception{ minifi::ExceptionType::GENERAL_EXCEPTION, get_last_socket_error().message() };
    }
  } else if (sa->sa_family == AF_INET6) {
    sockaddr_in6 sa_in6{};
    std::memcpy(&sa_in6, sa, sizeof(sockaddr_in6));
    result.resize(INET6_ADDRSTRLEN);
    if (inet_ntop(AF_INET6, &sa_in6.sin6_addr, result.data(), INET6_ADDRSTRLEN) == nullptr) {
      throw minifi::Exception{ minifi::ExceptionType::GENERAL_EXCEPTION, get_last_socket_error().message() };
    }
  } else {
    throw minifi::Exception{ minifi::ExceptionType::GENERAL_EXCEPTION, "sockaddr_ntop: unknown address family" };
  }
  result.resize(strlen(result.c_str()));  // discard remaining null bytes at the end
  return result;
}

}  // namespace org::apache::nifi::minifi::utils::net
