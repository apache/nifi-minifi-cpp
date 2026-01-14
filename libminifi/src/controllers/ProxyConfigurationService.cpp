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
#include "controllers/ProxyConfigurationService.h"

#include "utils/ParsingUtils.h"
#include "minifi-cpp/Exception.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::controllers {

void ProxyConfigurationService::initialize() {
  setSupportedProperties(Properties);
}

void ProxyConfigurationService::onEnable() {
  std::lock_guard lock(configuration_mutex_);
  proxy_configuration_.proxy_type = magic_enum::enum_cast<ProxyType>(getProperty(ProxyTypeProperty.name).value_or("HTTP")).value_or(ProxyType::HTTP);
  proxy_configuration_.proxy_host = getProperty(ProxyServerHost.name).value_or("");
  if (proxy_configuration_.proxy_host.empty()) {
    logger_->log_error("Proxy Server Host is required");
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Proxy Server Host is required");
  }
  if (auto proxy_port = getProperty(ProxyServerPort.name) | utils::andThen(parsing::parseIntegral<uint16_t>)) {
    proxy_configuration_.proxy_port = *proxy_port;
  }
  if (auto proxy_user = getProperty(ProxyUserName.name)) {
    proxy_configuration_.proxy_user = *proxy_user;
  }
  if (auto proxy_password = getProperty(ProxyUserPassword.name)) {
    proxy_configuration_.proxy_password = *proxy_password;
  }
}

REGISTER_RESOURCE(ProxyConfigurationService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
