// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  std::string proxy_user;
  if (auto proxy_user_prop = getProperty(ProxyUserName.name)) {
    proxy_user = *proxy_user_prop;
  }
  if (auto proxy_password = getProperty(ProxyUserPassword.name); !proxy_user.empty() && proxy_password) {
    proxy_configuration_.proxy_credentials = BasicAuthCredentials{.username = proxy_user, .password = *proxy_password};
  }
}

REGISTER_RESOURCE(ProxyConfigurationService, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
