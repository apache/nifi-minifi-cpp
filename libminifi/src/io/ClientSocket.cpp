/**
 *
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
#include "io/ClientSocket.h"
#ifndef WIN32
#include <netinet/tcp.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <unistd.h>
#else
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif /* !WIN32 */

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <memory>
#include <utility>
#include <vector>
#include <cerrno>
#include <string>
#include "Exception.h"
#include <system_error>
#include <cinttypes>
#include <Exception.h>
#include <utils/Deleters.h>
#include "io/validation.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "utils/GeneralUtils.h"
#include "utils/gsl.h"

namespace util = org::apache::nifi::minifi::utils;
namespace mio = org::apache::nifi::minifi::io;

namespace {
std::string get_last_getaddrinfo_err_str(int getaddrinfo_result) {
#ifdef WIN32
  (void)getaddrinfo_result;  // against unused warnings on windows
  return mio::get_last_socket_error_message();
#else
  return gai_strerror(getaddrinfo_result);
#endif /* WIN32 */
}

std::string sockaddr_ntop(const sockaddr* const sa) {
  std::string result;
  if (sa->sa_family == AF_INET) {
    sockaddr_in sa_in{};
    std::memcpy(reinterpret_cast<void*>(&sa_in), sa, sizeof(sockaddr_in));
    result.resize(INET_ADDRSTRLEN);
    if (inet_ntop(AF_INET, &sa_in.sin_addr, &result[0], INET_ADDRSTRLEN) == nullptr) {
      throw minifi::Exception{ minifi::ExceptionType::GENERAL_EXCEPTION, mio::get_last_socket_error_message() };
    }
  } else if (sa->sa_family == AF_INET6) {
    sockaddr_in6 sa_in6{};
    std::memcpy(reinterpret_cast<void*>(&sa_in6), sa, sizeof(sockaddr_in6));
    result.resize(INET6_ADDRSTRLEN);
    if (inet_ntop(AF_INET6, &sa_in6.sin6_addr, &result[0], INET6_ADDRSTRLEN) == nullptr) {
      throw minifi::Exception{ minifi::ExceptionType::GENERAL_EXCEPTION, mio::get_last_socket_error_message() };
    }
  } else {
    throw minifi::Exception{ minifi::ExceptionType::GENERAL_EXCEPTION, "sockaddr_ntop: unknown address family" };
  }
  result.resize(strlen(result.c_str()));  // discard remaining null bytes at the end
  return result;
}

template<typename T, typename Pred, typename Adv>
auto find_if_custom_linked_list(T* const list, const Adv advance_func, const Pred predicate) ->
    typename std::enable_if<std::is_convertible<decltype(advance_func(std::declval<T*>())), T*>::value && std::is_convertible<decltype(predicate(std::declval<T*>())), bool>::value, T*>::type
{
  for (T* it = list; it; it = advance_func(it)) {
    if (predicate(it)) return it;
  }
  return nullptr;
}

#ifndef WIN32
std::error_code bind_to_local_network_interface(const minifi::io::SocketDescriptor fd, const minifi::io::NetworkInterface& interface) {
  using ifaddrs_uniq_ptr = std::unique_ptr<ifaddrs, util::ifaddrs_deleter>;
  const auto if_list_ptr = []() -> ifaddrs_uniq_ptr {
    ifaddrs *list = nullptr;
    const auto get_ifa_success = getifaddrs(&list) == 0;
    assert(get_ifa_success || !list);
    (void)get_ifa_success;  // unused in release builds
    return ifaddrs_uniq_ptr{ list };
  }();
  if (!if_list_ptr) { return { errno, std::generic_category() }; }

  const auto advance_func = [](const ifaddrs *const p) { return p->ifa_next; };
  const auto predicate = [&interface](const ifaddrs *const item) {
    return item->ifa_addr && item->ifa_name && (item->ifa_addr->sa_family == AF_INET || item->ifa_addr->sa_family == AF_INET6)
        && item->ifa_name == interface.getInterface();
  };
  const auto *const itemFound = find_if_custom_linked_list(if_list_ptr.get(), advance_func, predicate);
  if (itemFound == nullptr) { return std::make_error_code(std::errc::no_such_device_or_address); }

  const socklen_t addrlen = itemFound->ifa_addr->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
  if (bind(fd, itemFound->ifa_addr, addrlen) != 0) { return { errno, std::generic_category() }; }
  return {};
}
#endif /* !WIN32 */

std::error_code set_non_blocking(const minifi::io::SocketDescriptor fd) noexcept {
#ifndef WIN32
  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
    return { errno, std::generic_category() };
  }
#else
  u_long iMode = 1;
  if (ioctlsocket(fd, FIONBIO, &iMode) == SOCKET_ERROR) {
    return { WSAGetLastError(), std::system_category() };
  }
#endif /* !WIN32 */
  return {};
}
}  // namespace

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

std::string get_last_socket_error_message() {
#ifdef WIN32
  const auto error_code = WSAGetLastError();
#else
  const auto error_code = errno;
#endif /* WIN32 */
  return std::system_category().message(error_code);
}

bool valid_socket(const SocketDescriptor fd) noexcept {
#ifdef WIN32
  return fd != INVALID_SOCKET && fd >= 0;
#else
  return fd >= 0;
#endif /* WIN32 */
}

Socket::Socket(const std::shared_ptr<SocketContext>& /*context*/, std::string hostname, const uint16_t port, const uint16_t listeners)
    : requested_hostname_(std::move(hostname)),
      port_(port),
      listeners_(listeners),
      logger_(logging::LoggerFactory<Socket>::getLogger()) {
  FD_ZERO(&total_list_);
  FD_ZERO(&read_fds_);
  initialize_socket();
}

Socket::Socket(const std::shared_ptr<SocketContext>& context, std::string hostname, const uint16_t port)
    : Socket(context, std::move(hostname), port, 0) {
}

// total_list_ and read_fds_ have to use parentheses for initialization due to CWG 1467
// http://www.open-std.org/jtc1/sc22/wg21/docs/cwg_defects.html#1467
// Language defect fix was applied to GCC 5 and Clang 4, but at the time of writing this comment, we support GCC 4.8
Socket::Socket(Socket &&other) noexcept
    : requested_hostname_{ std::move(other.requested_hostname_) },
      canonical_hostname_{ std::move(other.canonical_hostname_) },
      port_{ other.port_ },
      is_loopback_only_{ other.is_loopback_only_ },
      local_network_interface_{ std::move(other.local_network_interface_) },
      socket_file_descriptor_{ other.socket_file_descriptor_ },
      total_list_(other.total_list_),
      read_fds_(other.read_fds_),
      socket_max_{ other.socket_max_.load() },
      total_written_{ other.total_written_.load() },
      total_read_{ other.total_read_.load() },
      listeners_{ other.listeners_ },
      nonBlocking_{ other.nonBlocking_ },
      logger_{ other.logger_ }
{
  other = Socket{ {}, {}, {} };
}

Socket& Socket::operator=(Socket &&other) noexcept {
  if (&other == this) return *this;
  requested_hostname_ = util::exchange(other.requested_hostname_, "");
  canonical_hostname_ = util::exchange(other.canonical_hostname_, "");
  port_ = util::exchange(other.port_, 0);
  is_loopback_only_ = util::exchange(other.is_loopback_only_, false);
  local_network_interface_ = util::exchange(other.local_network_interface_, {});
  socket_file_descriptor_ = util::exchange(other.socket_file_descriptor_, INVALID_SOCKET);
  total_list_ = other.total_list_;
  FD_ZERO(&other.total_list_);
  read_fds_ = other.read_fds_;
  FD_ZERO(&other.read_fds_);
  socket_max_.exchange(other.socket_max_);
  other.socket_max_.exchange(0);
  total_written_.exchange(other.total_written_);
  other.total_written_.exchange(0);
  total_read_.exchange(other.total_read_);
  other.total_read_.exchange(0);
  listeners_ = util::exchange(other.listeners_, 0);
  nonBlocking_ = util::exchange(other.nonBlocking_, false);
  logger_ = other.logger_;
  return *this;
}

Socket::~Socket() {
  close();
}

void Socket::close() {
  if (valid_socket(socket_file_descriptor_)) {
    logging::LOG_DEBUG(logger_) << "Closing " << socket_file_descriptor_;
#ifdef WIN32
    closesocket(socket_file_descriptor_);
#else
    ::close(socket_file_descriptor_);
#endif
    socket_file_descriptor_ = INVALID_SOCKET;
  }
  if (total_written_ > 0) {
    local_network_interface_.log_write(gsl::narrow<uint32_t>(total_written_.load()));
    total_written_ = 0;
  }
  if (total_read_ > 0) {
    local_network_interface_.log_read(gsl::narrow<uint32_t>(total_read_.load()));
    total_read_ = 0;
  }
}

void Socket::setNonBlocking() {
  if (listeners_ <= 0) {
    nonBlocking_ = true;
  }
}

int8_t Socket::createConnection(const addrinfo* const destination_addresses) {
  for (const auto *current_addr = destination_addresses; current_addr; current_addr = current_addr->ai_next) {
    if (!valid_socket(socket_file_descriptor_ = socket(current_addr->ai_family, current_addr->ai_socktype, current_addr->ai_protocol))) {
      logger_->log_warn("socket: %s", get_last_socket_error_message());
      continue;
    }
    setSocketOptions(socket_file_descriptor_);

    if (listeners_ > 0) {
      // server socket
      const auto bind_result = bind(socket_file_descriptor_, current_addr->ai_addr, current_addr->ai_addrlen);
      if (bind_result == SOCKET_ERROR) {
        logger_->log_warn("bind: %s", get_last_socket_error_message());
        close();
        continue;
      }

      const auto listen_result = listen(socket_file_descriptor_, listeners_);
      if (listen_result == SOCKET_ERROR) {
        logger_->log_warn("listen: %s", get_last_socket_error_message());
        close();
        continue;
      }

      logger_->log_info("Listening on %s:%" PRIu16 " with backlog %" PRIu16, sockaddr_ntop(current_addr->ai_addr), port_, listeners_);
    } else {
      // client socket
#ifndef WIN32
      if (!local_network_interface_.getInterface().empty()) {
        const auto err = bind_to_local_network_interface(socket_file_descriptor_, local_network_interface_);
        if (err) logger_->log_info("Bind to interface %s failed %s", local_network_interface_.getInterface(), err.message());
        else logger_->log_info("Bind to interface %s", local_network_interface_.getInterface());
      }
#endif /* !WIN32 */

      const auto connect_result = connect(socket_file_descriptor_, current_addr->ai_addr, current_addr->ai_addrlen);
      if (connect_result == SOCKET_ERROR) {
        logger_->log_warn("Couldn't connect to %s:%" PRIu16 ": %s", sockaddr_ntop(current_addr->ai_addr), port_, get_last_socket_error_message());
        close();
        continue;
      }

      logger_->log_info("Connected to %s:%" PRIu16, sockaddr_ntop(current_addr->ai_addr), port_);
    }

    FD_SET(socket_file_descriptor_, &total_list_);
    socket_max_ = socket_file_descriptor_;
    return 0;
  }
  return -1;
}

int8_t Socket::createConnection(const addrinfo *, ip4addr &addr) {
  if (!valid_socket(socket_file_descriptor_ = socket(AF_INET, SOCK_STREAM, 0))) {
    logger_->log_error("error while connecting to server socket");
    return -1;
  }

  setSocketOptions(socket_file_descriptor_);

  if (listeners_ > 0) {
    // server socket
    sockaddr_in sa{};
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port_);
    sa.sin_addr.s_addr = htonl(is_loopback_only_ ? INADDR_LOOPBACK : INADDR_ANY);
    if (bind(socket_file_descriptor_, reinterpret_cast<const sockaddr*>(&sa), sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
      logger_->log_error("Could not bind to socket, reason %s", get_last_socket_error_message());
      return -1;
    }

    if (listen(socket_file_descriptor_, listeners_) == -1) {
      return -1;
    }
    logger_->log_debug("Created connection with %d listeners", listeners_);
  } else {
    // client socket
#ifndef WIN32
    if (!local_network_interface_.getInterface().empty()) {
      const auto err = bind_to_local_network_interface(socket_file_descriptor_, local_network_interface_);
      if (err) logger_->log_info("Bind to interface %s failed %s", local_network_interface_.getInterface(), err.message());
      else logger_->log_info("Bind to interface %s", local_network_interface_.getInterface());
    }
#endif /* !WIN32 */
    sockaddr_in sa_loc{};
    memset(&sa_loc, 0x00, sizeof(sa_loc));
    sa_loc.sin_family = AF_INET;
    sa_loc.sin_port = htons(port_);
    // use any address if you are connecting to the local machine for testing
    // otherwise we must use the requested hostname
    if (IsNullOrEmpty(requested_hostname_) || requested_hostname_ == "localhost") {
      sa_loc.sin_addr.s_addr = htonl(is_loopback_only_ ? INADDR_LOOPBACK : INADDR_ANY);
    } else {
#ifdef WIN32
      sa_loc.sin_addr.s_addr = addr.s_addr;
    }
    if (connect(socket_file_descriptor_, reinterpret_cast<const sockaddr*>(&sa_loc), sizeof(sockaddr_in)) == SOCKET_ERROR) {
      int err = WSAGetLastError();
      if (err == WSAEADDRNOTAVAIL) {
        logger_->log_error("invalid or unknown IP");
      } else if (err == WSAECONNREFUSED) {
        logger_->log_error("Connection refused");
      } else {
        logger_->log_error("Unknown error");
      }
#else
      sa_loc.sin_addr.s_addr = addr;
    }
    if (connect(socket_file_descriptor_, reinterpret_cast<const sockaddr *>(&sa_loc), sizeof(sockaddr_in)) < 0) {
#endif /* WIN32 */
      close();
      return -1;
    }
  }

  // add the listener to the total set
  FD_SET(socket_file_descriptor_, &total_list_);
  socket_max_ = socket_file_descriptor_;
  logger_->log_debug("Created connection with file descriptor %d", socket_file_descriptor_);
  return 0;
}

int Socket::initialize() {
  addrinfo hints{};
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;
  if (listeners_ > 0 && !is_loopback_only_)
    hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0; /* any protocol */

  const char* const gai_node = [this]() -> const char* {
    if (is_loopback_only_) return "localhost";
    if (!is_loopback_only_ && listeners_ > 0) return nullptr;  // all non-localhost server sockets listen on wildcard address
    if (!requested_hostname_.empty()) return requested_hostname_.c_str();
    return nullptr;
  }();
  const auto gai_service = std::to_string(port_);
  addrinfo* getaddrinfo_result = nullptr;
  const int errcode = getaddrinfo(gai_node, gai_service.c_str(), &hints, &getaddrinfo_result);
  const std::unique_ptr<addrinfo, util::addrinfo_deleter> addr_info{ getaddrinfo_result };
  getaddrinfo_result = nullptr;
  if (errcode != 0) {
    logger_->log_error("getaddrinfo: %s", get_last_getaddrinfo_err_str(errcode));
    return -1;
  }
  socket_file_descriptor_ = INVALID_SOCKET;

  // AI_CANONNAME always sets ai_canonname of the first addrinfo structure
  canonical_hostname_ = !IsNullOrEmpty(addr_info->ai_canonname) ? addr_info->ai_canonname : requested_hostname_;

  const auto conn_result = port_ > 0 ? createConnection(addr_info.get()) : -1;
  if (conn_result == 0 && nonBlocking_) {
    // Put the socket in non-blocking mode:
    const auto err = set_non_blocking(socket_file_descriptor_);
    if (err) logger_->log_info("Couldn't make socket non-blocking: %s", err.message());
    else logger_->log_debug("Successfully applied O_NONBLOCK to fd");
  }
  return conn_result;
}

int16_t Socket::select_descriptor(const uint16_t msec) {
  if (listeners_ == 0) {
    return socket_file_descriptor_;
  }

  struct timeval tv{};

  read_fds_ = total_list_;

  tv.tv_sec = msec / 1000;
  tv.tv_usec = (msec % 1000) * 1000;

  std::lock_guard<std::recursive_mutex> guard(selection_mutex_);

  if (msec > 0)
    select(socket_max_ + 1, &read_fds_, nullptr, nullptr, &tv);
  else
    select(socket_max_ + 1, &read_fds_, nullptr, nullptr, nullptr);

  for (int i = 0; i <= socket_max_; i++) {
    if (FD_ISSET(i, &read_fds_)) {
      if (i == socket_file_descriptor_) {
        if (listeners_ > 0) {
          struct sockaddr_storage remoteaddr;  // client address
          socklen_t addrlen = sizeof remoteaddr;
          int newfd = accept(socket_file_descriptor_, (struct sockaddr *) &remoteaddr, &addrlen);
          FD_SET(newfd, &total_list_);  // add to master set
          if (newfd > socket_max_) {    // keep track of the max
            socket_max_ = newfd;
          }
          return newfd;
        } else {
          return socket_file_descriptor_;
        }
        // we have a new connection
      } else {
        // data to be received on i
        return i;
      }
    }
  }

  logger_->log_debug("Could not find a suitable file descriptor or select timed out");

  return -1;
}

int16_t Socket::setSocketOptions(const SocketDescriptor sock) {
  int opt = 1;
#ifndef WIN32
#ifndef __MACH__
  if (setsockopt(sock, SOL_TCP, TCP_NODELAY, static_cast<void*>(&opt), sizeof(opt)) < 0) {
    logger_->log_error("setsockopt() TCP_NODELAY failed");
    ::close(sock);
    return -1;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0) {
    logger_->log_error("setsockopt() SO_REUSEADDR failed");
    ::close(sock);
    return -1;
  }

  int sndsize = 256 * 1024;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&sndsize), sizeof(sndsize)) < 0) {
    logger_->log_error("setsockopt() SO_SNDBUF failed");
    ::close(sock);
    return -1;
  }

#else
  if (listeners_ > 0) {
    // lose the pesky "address already in use" error message
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt), sizeof(opt)) < 0) {
      logger_->log_error("setsockopt() SO_REUSEADDR failed");
      ::close(sock);
      return -1;
    }
  }
#endif /* !__MACH__ */
#endif /* !WIN32 */
  return 0;
}

std::string Socket::getHostname() const {
  return canonical_hostname_;
}

// data stream overrides

int Socket::write(const uint8_t *value, int size) {
  gsl_Expects(size >= 0);

  int ret = 0, bytes = 0;

  int fd = select_descriptor(1000);
  if (fd < 0) { return -1; }
  while (bytes < size) {
    ret = send(fd, reinterpret_cast<const char*>(value) + bytes, size - bytes, 0);
    // check for errors
    if (ret <= 0) {
      utils::file::FileUtils::close(fd);
      logger_->log_error("Could not send to %d, error: %s", fd, get_last_socket_error_message());
      return ret;
    }
    bytes += ret;
  }

  if (ret)
    logger_->log_trace("Send data size %d over socket %d", size, fd);
  total_written_ += bytes;
  return bytes;
}

size_t Socket::read(uint8_t *buf, size_t buflen, bool retrieve_all_bytes) {
  size_t total_read = 0;
  while (buflen) {
    int16_t fd = select_descriptor(1000);
    if (fd < 0) {
      if (listeners_ <= 0) {
        logger_->log_debug("fd %d close %i", fd, buflen);
        utils::file::FileUtils::close(socket_file_descriptor_);
      }
      return STREAM_ERROR;
    }
    const auto bytes_read = recv(fd, reinterpret_cast<char*>(buf), buflen, 0);
    logger_->log_trace("Recv call %d", bytes_read);
    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        logger_->log_debug("Other side hung up on %d", fd);
      } else {
#ifdef WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
          // continue
          return static_cast<size_t>(-2);
        }
        logger_->log_error("Could not recv on %d (port %d), error code: %d", fd, port_, err);
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // continue
          return static_cast<size_t>(-2);
        }
        logger_->log_error("Could not recv on %d (port %d), error: %s", fd, port_, strerror(errno));

#endif  // WIN32
      }
      return STREAM_ERROR;
    }
    buflen -= gsl::narrow<size_t>(bytes_read);
    buf += bytes_read;
    total_read += gsl::narrow<size_t>(bytes_read);
    if (!retrieve_all_bytes) {
      break;
    }
  }
  total_read_ += total_read;
  return total_read;
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
