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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_NETWORKMANAGEMENTSERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_NETWORKMANAGEMENTSERVICE_H_

#include <iostream>
#include <memory>
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

/**
 * Purpose: network management service for interface binding/ipv6, etc
 */
class NetworkManagerService : public core::controller::ControllerService {
 public:
  explicit NetworkManagerService(const std::string &name, const std::string &id)
      : ControllerService(name, id),
        ip_v6_enabled_(false),
        logger_(logging::LoggerFactory<NetworkManagerService>::getLogger()) {
  }

  explicit NetworkManagerService(const std::string &name, uuid_t uuid = 0)
      : ControllerService(name, uuid),
        ip_v6_enabled_(false),
        logger_(logging::LoggerFactory<NetworkManagerService>::getLogger()) {
  }

  explicit NetworkManagerService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name, nullptr),
        logger_(logging::LoggerFactory<NetworkManagerService>::getLogger()) {
    setConfiguration(configuration);
    initialize();
  }

  static core::Property bindInterface;
  static core::Property ipv6Enable;

  void yield();

  bool isRunning();

  bool isWorkAvailable();

  void initialize();

  void onEnable();

  static const char* CONTEXT_SERVICE_NAME;

  std::string getBindInterface() {
    return bind_interface_;
  }

 protected:
  bool ip_v6_enabled_;
  std::string bind_interface_;

 private:
  std::shared_ptr<logging::Logger> logger_;

};

REGISTER_RESOURCE(NetworkManagerService);

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_NETWORKMANAGEMENTSERVICE_H_ */
