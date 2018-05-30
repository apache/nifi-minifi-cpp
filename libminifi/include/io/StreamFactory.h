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
#ifndef SOCKET_FACTORY_H
#define SOCKET_FACTORY_H

#include "properties/Configure.h"
#include "Sockets.h"
#include "utils/StringUtils.h"
#include "validation.h"
#include "controllers/SSLContextService.h"
#include "NetworkPrioritizer.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

class AbstractStreamFactory {
 public:
  virtual ~AbstractStreamFactory() {
  }

  virtual std::unique_ptr<Socket> createSocket(const std::string &host, const uint16_t port) = 0;

  virtual std::unique_ptr<Socket> createSecureSocket(const std::string &host, const uint16_t port, const std::shared_ptr<minifi::controllers::SSLContextService> &ssl_service) = 0;
};

/**
 Purpose: Due to the current design this is the only mechanism by which we can
 inject different socket types
 
 **/
class StreamFactory {
 public:

  /**
   * Creates a socket and returns a unique ptr
   *
   */
  std::unique_ptr<Socket> createSocket(const std::string &host, const uint16_t port, uint32_t estimated_size = 0) {
    auto socket = delegate_->createSocket(host, port);
    auto prioritizer_ = NetworkPrioritizerFactory::getInstance()->getPrioritizer();
    if (nullptr != prioritizer_) {
      auto &&ifc = prioritizer_->getInterface(estimated_size);
      if (ifc.getInterface().empty()) {
        return nullptr;
      } else {
        socket->setInterface(std::move(ifc));
      }
    }
    return socket;
  }

  /**
   * Creates a socket and returns a unique ptr
   *
   */
  std::unique_ptr<Socket> createSecureSocket(const std::string &host, const uint16_t port, const std::shared_ptr<minifi::controllers::SSLContextService> &ssl_service, uint32_t estimated_size = 0) {
    auto socket = delegate_->createSecureSocket(host, port, ssl_service);
    auto prioritizer_ = NetworkPrioritizerFactory::getInstance()->getPrioritizer();
    if (nullptr != prioritizer_) {
      auto &&ifc = prioritizer_->getInterface(estimated_size);
      if (ifc.getInterface().empty()) {
        return nullptr;
      } else {
        socket->setInterface(std::move(ifc));
      }
    }
    return socket;
  }

  static std::shared_ptr<StreamFactory> getInstance(const std::shared_ptr<Configure> &configuration) {
    // avoid invalid access
    static std::shared_ptr<StreamFactory> factory = std::shared_ptr<StreamFactory>(new StreamFactory(configuration));
    return factory;
  }

 protected:

  StreamFactory(const std::shared_ptr<Configure> &configure);

  std::shared_ptr<AbstractStreamFactory> delegate_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
