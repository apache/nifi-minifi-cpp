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
#ifndef SOCKET_FACTORY_H
#define SOCKET_FACTORY_H

#include "ClientSocket.h"
#include "TLSSocket.h"
#include "ClientSocket.h"
#include "Configure.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 Purpose: Due to the current design this is the only mechanism by which we can
 inject different socket types
 
 **/
class SocketFactory {
 public:

  /**
   * Build an instance, creating a memory fence, which
   * allows us to avoid locking. This is tantamount to double checked locking.
   * @returns new SocketFactory;
   */
  static SocketFactory *getInstance() {
    SocketFactory* atomic_context = context_instance_.load(
        std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (atomic_context == nullptr) {
      std::lock_guard<std::mutex> lock(context_mutex_);
      atomic_context = context_instance_.load(std::memory_order_relaxed);
      if (atomic_context == nullptr) {
        atomic_context = new SocketFactory();
        std::atomic_thread_fence(std::memory_order_release);
        context_instance_.store(atomic_context, std::memory_order_relaxed);
      }
    }
    return atomic_context;
  }

  /**
   * Creates a socket and returns a unique ptr
   *
   */
  std::unique_ptr<Socket> createSocket(const std::string &host,
                                       const uint16_t port) {
    Socket *socket = 0;
    if (is_secure_) {
      socket = new TLSSocket(host, port);
    } else {
      socket = new Socket(host, port);
    }
    return std::unique_ptr<Socket>(socket);
  }
 protected:
  SocketFactory()
      : configure_(Configure::getConfigure()) {
    std::string secureStr;
    is_secure_ = false;
    if (configure_->get(Configure::nifi_remote_input_secure, secureStr)) {
      org::apache::nifi::minifi::utils::StringUtils::StringToBool(secureStr, is_secure_);
    }
  }

  bool is_secure_;
  static std::atomic<SocketFactory*> context_instance_;
  static std::mutex context_mutex_;

  Configure *configure_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
