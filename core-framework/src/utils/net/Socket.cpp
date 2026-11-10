// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
#include "minifi-cpp/Exception.h"

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
