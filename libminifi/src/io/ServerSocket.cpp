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
#include "io/ServerSocket.h"
#include "io/DescriptorStream.h"

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

ServerSocket::ServerSocket(const std::shared_ptr<SocketContext> &context, const std::string &hostname, const uint16_t port, const uint16_t listeners = -1)
    : Socket(context, hostname, port, listeners),
      running_(true),
      logger_(logging::LoggerFactory<ServerSocket>::getLogger()) {
}

ServerSocket::~ServerSocket() {
  running_ = false;
  if (server_read_thread_.joinable())
    server_read_thread_.join();
}

/**
 * Initializes the socket
 * @return result of the creation operation.
 */
void ServerSocket::registerCallback(std::function<bool()> accept_function, std::function<void(io::BaseStream *)> handler) {
  auto fx = [this](std::function<bool()> accept_function, std::function<void(io::BaseStream *stream)> handler) {
    while (running_) {
      int fd = select_descriptor(1000);
      if (fd >= 0) {
        io::DescriptorStream stream(fd);
        handler(&stream);
        close_fd(fd);
      }
    }
  };
  server_read_thread_ = std::thread(fx, accept_function, handler);
}

void ServerSocket::close_fd(int fd) {
  std::lock_guard<std::recursive_mutex> guard(selection_mutex_);
  close(fd);
  FD_CLR(fd, &total_list_);
}

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
