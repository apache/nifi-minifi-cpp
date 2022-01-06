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
#include <system_error>
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <WinSock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif /* WIN32 */
#include "nonstd/expected.hpp"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::utils::net {
#ifdef WIN32
using SocketDescriptor = SOCKET;
using ip4addr = in_addr;
inline constexpr SocketDescriptor InvalidSocket = INVALID_SOCKET;
constexpr int SocketError = SOCKET_ERROR;
#else
using SocketDescriptor = int;
using ip4addr = in_addr_t;
#undef INVALID_SOCKET
inline constexpr SocketDescriptor InvalidSocket = -1;
#undef SOCKET_ERROR
inline constexpr int SocketError = -1;
#endif /* WIN32 */

/**
 * Return the last socket error code, based on errno on posix and WSAGetLastError() on windows.
 */
std::error_code get_last_socket_error();

inline void close_socket(SocketDescriptor sockfd) {
#ifdef WIN32
  closesocket(sockfd);
#else
  ::close(sockfd);
#endif
}

class UniqueSocketHandle {
 public:
  explicit UniqueSocketHandle(SocketDescriptor owner_sockfd) noexcept
      :owner_sockfd_(owner_sockfd)
  {}

  UniqueSocketHandle(const UniqueSocketHandle&) = delete;
  UniqueSocketHandle(UniqueSocketHandle&& other) noexcept
    :owner_sockfd_{std::exchange(other.owner_sockfd_, InvalidSocket)}
  {}
  ~UniqueSocketHandle() noexcept {
    if (owner_sockfd_ != InvalidSocket) close_socket(owner_sockfd_);
  }
  UniqueSocketHandle& operator=(const UniqueSocketHandle&) = delete;
  UniqueSocketHandle& operator=(UniqueSocketHandle&& other) noexcept {
    if (&other == this) return *this;
    if (owner_sockfd_ != InvalidSocket) close_socket(owner_sockfd_);
    owner_sockfd_ = std::exchange(other.owner_sockfd_, InvalidSocket);
    return *this;
  }

  [[nodiscard]] SocketDescriptor get() const noexcept { return owner_sockfd_; }
  [[nodiscard]] SocketDescriptor release() noexcept { return std::exchange(owner_sockfd_, InvalidSocket); }
  explicit operator bool() const noexcept { return owner_sockfd_ != InvalidSocket; }
  bool operator==(UniqueSocketHandle other) const noexcept { return owner_sockfd_ == other.owner_sockfd_; }

 private:
  SocketDescriptor owner_sockfd_;
};

struct OpenSocketResult {
  UniqueSocketHandle socket_;
  gsl::not_null<const addrinfo*> selected_name;
};

/**
 * Iterate through getaddrinfo_result and try to call socket() until it succeeds
 * @param getaddrinfo_result
 * @return The file descriptor and the selected list element on success, or nullopt on error. Use get_last_socket_error_message() to get the error message.
 */
nonstd::expected<OpenSocketResult, std::error_code> open_socket(gsl::not_null<const addrinfo*> getaddrinfo_result);
inline nonstd::expected<OpenSocketResult, std::error_code> open_socket(const addrinfo& getaddrinfo_result) { return open_socket(gsl::make_not_null(&getaddrinfo_result)); }


std::string sockaddr_ntop(const sockaddr* sa);

}  // namespace org::apache::nifi::minifi::utils::net
