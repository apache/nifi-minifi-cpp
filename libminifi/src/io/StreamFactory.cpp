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
#include "io/StreamFactory.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#ifdef OPENSSL_SUPPORT
#include "io/tls/TLSSocket.h"
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {


/**
 * Purpose: Socket Creator is a class that will determine if the provided socket type
 * exists per the compilation parameters
 */

template<typename T, typename V>
class SocketCreator : public AbstractStreamFactory {
  template<bool cond, typename U>
  using TypeCheck = typename std::enable_if< cond, U >::type;

  template<bool cond, typename Q>
  using ContextTypeCheck = typename std::enable_if< cond, Q >::type;

 public:
  template<typename Q = V>
  ContextTypeCheck<true, std::shared_ptr<Q>> create(std::shared_ptr<Configure> configure) {
    return std::make_shared<V>(configure);
  }
  template<typename Q = V>
  ContextTypeCheck<false, std::shared_ptr<Q>> create(std::shared_ptr<Configure> configure) {
    return std::make_shared<SocketContext>(configure);
  }

  SocketCreator<T, V>(std::shared_ptr<Configure> configure) {
    context_ = create(configure);
  }

  template<typename U = T>
  TypeCheck<true, U> *create(const std::string &host, const uint16_t port) {
    return new T(context_, host, port);
  }
  template<typename U = T>
  TypeCheck<false, U> *create(const std::string &host, const uint16_t port) {
    return new Socket(context_, host, port);
  }

  std::unique_ptr<Socket> createSocket(const std::string &host, const uint16_t port) {
    T *socket = create(host, port);
    return std::unique_ptr<Socket>(socket);
  }

 private:
  std::shared_ptr<V> context_;
};

// std::atomic<StreamFactory*> StreamFactory::context_instance_;
// std::mutex StreamFactory::context_mutex_;
StreamFactory::StreamFactory(std::shared_ptr<Configure> configure) {
  std::string secureStr;
  bool is_secure = false;
  if (configure->get(Configure::nifi_remote_input_secure, secureStr)) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(secureStr, is_secure);
    delegate_ = std::make_shared<SocketCreator<TLSSocket, TLSContext>>(configure);
  } else {
    delegate_ = std::make_shared<SocketCreator<Socket, SocketContext>>(configure);
  }
}
} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
