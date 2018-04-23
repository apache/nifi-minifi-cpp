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
#include "controllers/NetworkManagementService.h"
#include <utility>
#include <limits>
#include <string>
#include <vector>
#include <set>
#include "utils/StringUtils.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

const char *NetworkManagerService::CONTEXT_SERVICE_NAME = "NetworkManagerService";
core::Property NetworkManagerService::bindInterface("Bind Interface", "Bind socket to the specific interface");
core::Property NetworkManagerService::ipv6Enable("IPv6", "Enable IPv6");

void NetworkManagerService::initialize() {
  ControllerService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(bindInterface);
  supportedProperties.insert(ipv6Enable);
  setSupportedProperties(supportedProperties);
}

void NetworkManagerService::yield() {
}

bool NetworkManagerService::isRunning() {
  return getState() == core::controller::ControllerServiceState::ENABLED;
}

bool NetworkManagerService::isWorkAvailable() {
  return false;
}

void NetworkManagerService::onEnable() {
  std::string value;
  if (getProperty(bindInterface.getName(), value) && !value.empty()) {
    bind_interface_ = value;
    logger_->log_info("Network Manager bind socket to interface %s", bind_interface_);
  }
  value = "";
  if (getProperty(ipv6Enable.getName(), value) && !value.empty()) {
    org::apache::nifi::minifi::utils::StringUtils::StringToBool(value, ip_v6_enabled_);
    logger_->log_info("Network Manager IPV6 %d", ip_v6_enabled_);
  }
}
} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
