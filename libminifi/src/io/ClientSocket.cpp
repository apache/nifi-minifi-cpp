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
#include <netinet/tcp.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>
#include <cerrno>
#include <iostream>
#include <string>
#include "io/validation.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

Socket::Socket(const std::shared_ptr<SocketContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners = -1)
    : requested_hostname_(hostname),
      port_(port),
      addr_info_(0),
      socket_file_descriptor_(-1),
      socket_max_(0),
      total_written_(0),
      total_read_(0),
      is_loopback_only_(false),
      listeners_(listeners),
      canonical_hostname_(""),
      nonBlocking_(false),
      logger_(logging::LoggerFactory<Socket>::getLogger()) {
  FD_ZERO(&total_list_);
  FD_ZERO(&read_fds_);
}

Socket::Socket(const std::shared_ptr<SocketContext> &context, const std::string &hostname, const uint16_t port)
    : Socket(context, hostname, port, 0) {
}

Socket::Socket(const Socket &&other)
    : requested_hostname_(std::move(other.requested_hostname_)),
      port_(std::move(other.port_)),
      is_loopback_only_(false),
      addr_info_(std::move(other.addr_info_)),
      socket_file_descriptor_(other.socket_file_descriptor_),
      socket_max_(other.socket_max_.load()),
      listeners_(other.listeners_),
      total_list_(other.total_list_),
      read_fds_(other.read_fds_),
      canonical_hostname_(std::move(other.canonical_hostname_)),
      nonBlocking_(false),
      logger_(std::move(other.logger_)) {
  total_written_ = other.total_written_.load();
  total_read_ = other.total_read_.load();
}

Socket::~Socket() {
  closeStream();
}

void Socket::closeStream() {
  if (0 != addr_info_) {
    freeaddrinfo(addr_info_);
    addr_info_ = 0;
  }
  if (socket_file_descriptor_ >= 0) {
    logging::LOG_DEBUG(logger_) << "Closing " << socket_file_descriptor_;
    close(socket_file_descriptor_);
    socket_file_descriptor_ = -1;
  }
  if (total_written_ > 0) {
    local_network_interface_.log_write(total_written_);
    total_written_ = 0;
  }
  if (total_read_ > 0) {
    local_network_interface_.log_read(total_read_);
    total_read_ = 0;
  }
}

void Socket::setNonBlocking() {
  if (listeners_ <= 0) {
    nonBlocking_ = true;
  }
}

int8_t Socket::createConnection(const addrinfo *p, in_addr_t &addr) {
  if ((socket_file_descriptor_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
    logger_->log_error("error while connecting to server socket");
    return -1;
  }

  setSocketOptions(socket_file_descriptor_);

  if (listeners_ <= 0 && !local_network_interface_.getInterface().empty()) {
    // bind to local network interface
    ifaddrs* list = NULL;
    ifaddrs* item = NULL;
    ifaddrs* itemFound = NULL;
    int result = getifaddrs(&list);
    if (result == 0) {
      item = list;
      while (item) {
        if ((item->ifa_addr != NULL) && (item->ifa_name != NULL) && (AF_INET == item->ifa_addr->sa_family)) {
          if (strcmp(item->ifa_name, local_network_interface_.getInterface().c_str()) == 0) {
            itemFound = item;
            break;
          }
        }
        item = item->ifa_next;
      }

      if (itemFound != NULL) {
        result = bind(socket_file_descriptor_, itemFound->ifa_addr, sizeof(struct sockaddr_in));
        if (result < 0)
          logger_->log_info("Bind to interface %s failed %s", local_network_interface_.getInterface(), strerror(errno));
        else
          logger_->log_info("Bind to interface %s", local_network_interface_.getInterface());
      }
      freeifaddrs(list);
    }
  }

  if (listeners_ > 0) {
    struct sockaddr_in *sa_loc = (struct sockaddr_in*) p->ai_addr;
    sa_loc->sin_family = AF_INET;
    sa_loc->sin_port = htons(port_);
    if (is_loopback_only_) {
      sa_loc->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
      sa_loc->sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (bind(socket_file_descriptor_, p->ai_addr, p->ai_addrlen) == -1) {
      logger_->log_error("Could not bind to socket, reason %s", strerror(errno));
      return -1;
    }
  }
  {
    if (listeners_ <= 0) {
      struct sockaddr_in *sa_loc = (struct sockaddr_in*) p->ai_addr;
      sa_loc->sin_family = AF_INET;
      sa_loc->sin_port = htons(port_);
      // use any address if you are connecting to the local machine for testing
      // otherwise we must use the requested hostname
      if (IsNullOrEmpty(requested_hostname_) || requested_hostname_ == "localhost") {
        if (is_loopback_only_) {
          sa_loc->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        } else {
          sa_loc->sin_addr.s_addr = htonl(INADDR_ANY);
        }
      } else {
        sa_loc->sin_addr.s_addr = addr;
      }
      if (connect(socket_file_descriptor_, p->ai_addr, p->ai_addrlen) == -1) {
        close(socket_file_descriptor_);
        socket_file_descriptor_ = -1;
        return -1;
      }
    }
  }

  // listen
  if (listeners_ > 0) {
    if (listen(socket_file_descriptor_, listeners_) == -1) {
      return -1;
    } else {
      logger_->log_debug("Created connection with %d listeners", listeners_);
    }
  }
  // add the listener to the total set
  FD_SET(socket_file_descriptor_, &total_list_);
  socket_max_ = socket_file_descriptor_;
  logger_->log_debug("Created connection with file descriptor %d", socket_file_descriptor_);
  return 0;
}

int16_t Socket::initialize() {
  addrinfo hints = { sizeof(addrinfo) };
  memset(&hints, 0, sizeof hints);  // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;
  if (listeners_ > 0)
    hints.ai_flags |= AI_PASSIVE;
  hints.ai_protocol = 0; /* any protocol */

  int errcode = getaddrinfo(requested_hostname_.c_str(), 0, &hints, &addr_info_);

  if (errcode != 0) {
    logger_->log_error("Saw error during getaddrinfo, error: %s", strerror(errno));
    return -1;
  }

  socket_file_descriptor_ = -1;

  in_addr_t addr;
  struct hostent *h;
#ifdef __MACH__
  h = gethostbyname(requested_hostname_.c_str());
#else
  const char *host;

  host = requested_hostname_.c_str();
  char buf[1024];
  struct hostent he;
  int hh_errno;
  gethostbyname_r(host, &he, buf, sizeof(buf), &h, &hh_errno);
#endif
  if (h == nullptr) {
    return -1;
  }
  memcpy(reinterpret_cast<char*>(&addr), h->h_addr_list[0], h->h_length);

  auto p = addr_info_;
  for (; p != NULL; p = p->ai_next) {
    if (IsNullOrEmpty(canonical_hostname_)) {
      if (!IsNullOrEmpty(p) && !IsNullOrEmpty(p->ai_canonname))
        canonical_hostname_ = p->ai_canonname;
    }
    // we've successfully connected
    if (port_ > 0 && createConnection(p, addr) >= 0) {
      // Put the socket in non-blocking mode:
      if (nonBlocking_) {
        if (fcntl(socket_file_descriptor_, F_SETFL, O_NONBLOCK) < 0) {
          // handle error
          logger_->log_error("Could not create non blocking to socket", strerror(errno));
        } else {
          logger_->log_debug("Successfully applied O_NONBLOCK to fd");
        }
      }
      logger_->log_debug("Successfully created connection");
      return 0;
      break;
    }
  }

  logger_->log_debug("Could not find device for our connection");
  return -1;
}

int16_t Socket::select_descriptor(const uint16_t msec) {
  if (listeners_ == 0) {
    return socket_file_descriptor_;
  }

  struct timeval tv;

  read_fds_ = total_list_;

  tv.tv_sec = msec / 1000;
  tv.tv_usec = (msec % 1000) * 1000;

  std::lock_guard<std::recursive_mutex> guard(selection_mutex_);

  if (msec > 0)
    select(socket_max_ + 1, &read_fds_, NULL, NULL, &tv);
  else
    select(socket_max_ + 1, &read_fds_, NULL, NULL, NULL);

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

int16_t Socket::setSocketOptions(const int sock) {
  int opt = 1;
#ifndef __MACH__
  if (setsockopt(sock, SOL_TCP, TCP_NODELAY, static_cast<void*>(&opt), sizeof(opt)) < 0) {
    logger_->log_error("setsockopt() TCP_NODELAY failed");
    close(sock);
    return -1;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt)) < 0) {
    logger_->log_error("setsockopt() SO_REUSEADDR failed");
    close(sock);
    return -1;
  }

  int sndsize = 256 * 1024;
  if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&sndsize), sizeof(sndsize)) < 0) {
    logger_->log_error("setsockopt() SO_SNDBUF failed");
    close(sock);
    return -1;
  }

#else
  if (listeners_ > 0) {
    // lose the pesky "address already in use" error message
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&opt), sizeof(opt)) < 0) {
      logger_->log_error("setsockopt() SO_REUSEADDR failed");
      close(sock);
      return -1;
    }
  }
#endif
  return 0;
}

std::string Socket::getHostname() const {
  return canonical_hostname_;
}

int Socket::writeData(std::vector<uint8_t> &buf, int buflen) {
  if (static_cast<int>(buf.capacity()) < buflen)
    return -1;
  return writeData(reinterpret_cast<uint8_t *>(&buf[0]), buflen);
}

// data stream overrides

int Socket::writeData(uint8_t *value, int size) {
  int ret = 0, bytes = 0;

  int fd = select_descriptor(1000);
  while (bytes < size) {
    ret = send(fd, value + bytes, size - bytes, 0);
    // check for errors
    if (ret <= 0) {
      close(fd);
      logger_->log_error("Could not send to %d, error: %s", fd, strerror(errno));
      return ret;
    }
    bytes += ret;
  }

  if (ret)
    logger_->log_trace("Send data size %d over socket %d", size, fd);
  total_written_ += bytes;
  return bytes;
}

template<typename T>
inline std::vector<uint8_t> Socket::readBuffer(const T& t) {
  std::vector<uint8_t> buf;
  buf.resize(sizeof t);
  readData(reinterpret_cast<uint8_t *>(&buf[0]), sizeof(t));
  return buf;
}

int Socket::write(uint64_t base_value, bool is_little_endian) {
  return Serializable::write(base_value, this, is_little_endian);
}

int Socket::write(uint32_t base_value, bool is_little_endian) {
  return Serializable::write(base_value, this, is_little_endian);
}

int Socket::write(uint16_t base_value, bool is_little_endian) {
  return Serializable::write(base_value, this, is_little_endian);
}

int Socket::read(uint64_t &value, bool is_little_endian) {
  auto buf = readBuffer(value);

  if (is_little_endian) {
    value = ((uint64_t) buf[0] << 56) | ((uint64_t) (buf[1] & 255) << 48) | ((uint64_t) (buf[2] & 255) << 40) | ((uint64_t) (buf[3] & 255) << 32) | ((uint64_t) (buf[4] & 255) << 24)
        | ((uint64_t) (buf[5] & 255) << 16) | ((uint64_t) (buf[6] & 255) << 8) | ((uint64_t) (buf[7] & 255) << 0);
  } else {
    value = ((uint64_t) buf[0] << 0) | ((uint64_t) (buf[1] & 255) << 8) | ((uint64_t) (buf[2] & 255) << 16) | ((uint64_t) (buf[3] & 255) << 24) | ((uint64_t) (buf[4] & 255) << 32)
        | ((uint64_t) (buf[5] & 255) << 40) | ((uint64_t) (buf[6] & 255) << 48) | ((uint64_t) (buf[7] & 255) << 56);
  }
  return sizeof(value);
}

int Socket::read(uint32_t &value, bool is_little_endian) {
  auto buf = readBuffer(value);

  if (is_little_endian) {
    value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
  } else {
    value = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
  }
  return sizeof(value);
}

int Socket::read(uint16_t &value, bool is_little_endian) {
  auto buf = readBuffer(value);

  if (is_little_endian) {
    value = (buf[0] << 8) | buf[1];
  } else {
    value = buf[0] | buf[1] << 8;
  }
  return sizeof(value);
}

int Socket::readData(std::vector<uint8_t> &buf, int buflen, bool retrieve_all_bytes) {
  if (static_cast<int>(buf.capacity()) < buflen) {
    buf.resize(buflen);
  }
  return readData(reinterpret_cast<uint8_t*>(&buf[0]), buflen, retrieve_all_bytes);
}

int Socket::readData(uint8_t *buf, int buflen, bool retrieve_all_bytes) {
  int32_t total_read = 0;
  while (buflen) {
    int16_t fd = select_descriptor(1000);
    if (fd < 0) {
      if (listeners_ <= 0) {
        logger_->log_debug("fd %d close %i", fd, buflen);
        close(socket_file_descriptor_);
      }
      return -1;
    }
    int bytes_read = recv(fd, buf, buflen, 0);
    logger_->log_trace("Recv call %d", bytes_read);
    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        logger_->log_debug("Other side hung up on %d", fd);
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // continue
          return -2;
        }
        logger_->log_error("Could not recv on %d ( port %d), error: %s", fd, port_, strerror(errno));
      }
      return -1;
    }
    buflen -= bytes_read;
    buf += bytes_read;
    total_read += bytes_read;
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
