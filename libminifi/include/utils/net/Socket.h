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
#include <string>

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
 * Return the last socket error message, based on errno on posix and WSAGetLastError() on windows
 */
std::string get_last_socket_error_message();

inline void close_socket(SocketDescriptor sockfd) {
#ifdef WIN32
  closesocket(sockfd);
#else
  ::close(sockfd);
#endif
}

std::string sockaddr_ntop(const sockaddr* sa);

}  // namespace org::apache::nifi::minifi::utils::net
