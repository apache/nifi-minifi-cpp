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
#include "controllers/NetworkManagementService.h"
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
  std::unique_ptr<Socket> createSocket(const std::string &host, const uint16_t port) {
    std::unique_ptr<Socket> socket = delegate_->createSocket(host, port);
    if (network_mgnt_service_) {
      std::string interface = network_mgnt_service_->getBindInterface();
      if (!interface.empty())
        socket->setInterface(interface);
    }
    return socket;
  }

  /**
   * Creates a socket and returns a unique ptr
   *
   */
  std::unique_ptr<Socket> createSecureSocket(const std::string &host, const uint16_t port, const std::shared_ptr<minifi::controllers::SSLContextService> &ssl_service) {
    std::unique_ptr<Socket> socket = delegate_->createSecureSocket(host, port, ssl_service);
    if (network_mgnt_service_) {
      std::string interface = network_mgnt_service_->getBindInterface();
      if (!interface.empty())
        socket->setInterface(interface);
    }
    return socket;
  }

  StreamFactory(const std::shared_ptr<Configure> &configure);

  void setNetworkManagerService(std::shared_ptr<minifi::controllers::NetworkManagerService> &network_mgnt_service) {
    network_mgnt_service_ = network_mgnt_service;
  }

  std::shared_ptr<minifi::controllers::NetworkManagerService> getNetworkManagerService() {
    return network_mgnt_service_;
  }

 protected:
  std::shared_ptr<AbstractStreamFactory> delegate_;
  std::shared_ptr<minifi::controllers::NetworkManagerService> network_mgnt_service_;
};

} /* namespace io */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
