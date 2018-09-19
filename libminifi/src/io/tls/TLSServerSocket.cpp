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
#include "io/tls/SecureDescriptorStream.h"
#include "io/tls/TLSServerSocket.h"

#include <sys/types.h>
#ifndef WIN32
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <cstdio>
#include <memory>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>
#include <cerrno>
#include <iostream>
#include <algorithm>
#include <string>
#include "io/validation.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

TLSServerSocket::TLSServerSocket(const std::shared_ptr<TLSContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners = -1)
    : TLSSocket(context, hostname, port, listeners),
      running_(true),
      logger_(logging::LoggerFactory<TLSServerSocket>::getLogger()) {
}

TLSServerSocket::~TLSServerSocket() {
  running_ = false;
  if (server_read_thread_.joinable())
    server_read_thread_.join();
}

/**
 * Initializes the socket
 * @return result of the creation operation.
 */
void TLSServerSocket::registerCallback(std::function<bool()> accept_function, std::function<void(io::BaseStream *)> handler) {
  auto fx = [this](std::function<bool()> accept_function, std::function<void(io::BaseStream *)> handler) {
    while (running_) {
      int fd = select_descriptor(1000);
      if (fd >= 0) {
        auto ssl = get_ssl(fd);
        if (ssl != nullptr) {
          io::SecureDescriptorStream stream(fd, ssl);
          handler(&stream);
          close_fd(fd);
        }
      }
    }
  };
  server_read_thread_ = std::thread(fx, accept_function, handler);
}
/**
 * Initializes the socket
 * @return result of the creation operation.
 */
void TLSServerSocket::registerCallback(std::function<bool()> accept_function, std::function<int(std::vector<uint8_t>*, int *)> handler) {
  fx = [this](std::function<bool()> accept_function, std::function<int(std::vector<uint8_t>*, int *)> handler) {
    int ret = 0;
    std::vector<int> fds;
    int size;
    while (accept_function()) {
      int fd = select_descriptor(3000);
      if (fd > 0) {
        int fd_remove = 0;
        std::vector<uint8_t> data;
        if ( handler(&data, &size) > 0 ) {
          ret = writeData(data.data(), size, fd);
          if (ret < 0) {
            close_ssl(fd_remove);
          } else {
            fds.push_back(fd);
          }
        }
      } else {
        int fd_remove = 0;
        for (auto &&fd : fds) {
          std::vector<uint8_t> data;
          if ( handler(&data, &size) > 0 ) {
            ret = writeData(data.data(), size, fd);
            if (ret < 0) {
              fd_remove = fd;
              break;
            }
          }
        }
        if (fd_remove > 0) {
          close_ssl(fd_remove);
          fds.erase(std::remove(fds.begin(), fds.end(), fd_remove), fds.end());
        }
      }
    }
    for (auto &&fd : fds) {
      close_ssl(fd);
    }
  };
  server_read_thread_ = std::thread(fx, accept_function, handler);
}

void TLSServerSocket::close_fd(int fd) {
  std::lock_guard<std::recursive_mutex> guard(selection_mutex_);
  close_ssl(fd);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
