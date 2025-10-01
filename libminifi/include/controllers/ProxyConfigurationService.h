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
#pragma once

#include <mutex>

#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"
#include "core/controller/ControllerService.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"

namespace org::apache::nifi::minifi::controllers {

class ProxyConfigurationService : public core::controller::ControllerServiceImpl, public ProxyConfigurationServiceInterface {
 public:
  explicit ProxyConfigurationService(std::string_view name, const utils::Identifier& uuid = {})
      : ControllerServiceImpl(name, uuid) {
  }

  MINIFIAPI static constexpr const char* Description = "Provides a set of configurations for different MiNiFi C++ components to use a proxy server.";

  MINIFIAPI static constexpr auto ProxyServerHost = core::PropertyDefinitionBuilder<>::createProperty("Proxy Server Host")
      .withDescription("Proxy server hostname or ip-address.")
      .isRequired(true)
      .build();
  MINIFIAPI static constexpr auto ProxyServerPort = core::PropertyDefinitionBuilder<>::createProperty("Proxy Server Port")
      .withDescription("Proxy server port number.")
      .withValidator(core::StandardPropertyValidators::PORT_VALIDATOR)
      .build();
  MINIFIAPI static constexpr auto ProxyUserName = core::PropertyDefinitionBuilder<>::createProperty("Proxy User Name")
      .withDescription("The name of the proxy client for user authentication.")
      .build();
  MINIFIAPI static constexpr auto ProxyUserPassword = core::PropertyDefinitionBuilder<>::createProperty("Proxy User Password")
      .withDescription("The password of the proxy client for user authentication.")
      .isSensitive(true)
      .build();
  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    ProxyServerHost,
    ProxyServerPort,
    ProxyUserName,
    ProxyUserPassword
  });

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;
  MINIFIAPI static constexpr auto ImplementsApis = std::array{ ProxyConfigurationServiceInterface::ProvidesApi };
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void initialize() override;
  void onEnable() override;

  ProxyConfiguration getProxyConfiguration() const override {
    std::lock_guard lock(configuration_mutex_);
    return proxy_configuration_;
  }

 private:
  mutable std::mutex configuration_mutex_;
  ProxyConfiguration proxy_configuration_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ProxyConfigurationService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::controllers
