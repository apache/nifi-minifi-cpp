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
#ifndef LIBMINIFI_INCLUDE_UTILS_DELETERS_H
#define LIBMINIFI_INCLUDE_UTILS_DELETERS_H

#include <cstdlib>
#ifdef WIN32
#include <WS2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#endif /* WIN32 */

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

struct FreeDeleter {
  void operator()(void* const ptr) const noexcept {
    // free(null) is guaranteed to be safe, so no need to be defensive.
    free(ptr);
  }
};

struct addrinfo_deleter {
  void operator()(addrinfo* const p) const noexcept {
    freeaddrinfo(p);
  }
};

#ifndef WIN32
struct ifaddrs_deleter {
  void operator()(ifaddrs* const p) const noexcept {
    freeifaddrs(p);
  }
};
#endif /* !WIN32 */

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif // LIBMINIFI_INCLUDE_UTILS_DELETERS_H