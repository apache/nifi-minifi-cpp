/**
 * @file AzureStorageProcessorBase.cpp
 * AzureStorageProcessorBase class implementation
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

#include "AzureStorageProcessorBase.h"

#include <memory>
#include <string>

#include "minifi-cpp/core/ProcessContext.h"
#include "controllerservices/AzureStorageCredentialsService.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::azure::processors {

void AzureStorageProcessorBase::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  auto proxy_controller_service = minifi::utils::parseOptionalControllerService<minifi::controllers::ProxyConfigurationServiceInterface>(context, ProxyConfigurationService, getUUID());
  if (proxy_controller_service) {
    logger_->log_debug("Proxy configuration is set for Azure Storage processor");
    proxy_configuration_ = minifi::controllers::ProxyConfiguration{
      .proxy_type = minifi::controllers::ProxyType::HTTP,
      .proxy_host = proxy_controller_service->getHost(),
      .proxy_port = proxy_controller_service->getPort(),
      .proxy_user = proxy_controller_service->getUsername(),
      .proxy_password = proxy_controller_service->getPassword()
    };
  }
}

std::tuple<AzureStorageProcessorBase::GetCredentialsFromControllerResult, std::optional<storage::AzureStorageCredentials>> AzureStorageProcessorBase::getCredentialsFromControllerService(
    core::ProcessContext &context) const {
  std::string service_name = context.getProperty(AzureStorageCredentialsService).value_or("");
  if (service_name.empty()) {
    return std::make_tuple(GetCredentialsFromControllerResult::CONTROLLER_NAME_EMPTY, std::nullopt);
  }

  std::shared_ptr<core::controller::ControllerService> service = context.getControllerService(service_name, getUUID());
  if (nullptr == service) {
    logger_->log_error("Azure Storage credentials service with name: '{}' could not be found", service_name);
    return std::make_tuple(GetCredentialsFromControllerResult::CONTROLLER_NAME_INVALID, std::nullopt);
  }

  auto azure_credentials_service = std::dynamic_pointer_cast<minifi::azure::controllers::AzureStorageCredentialsService>(service);
  if (!azure_credentials_service) {
    logger_->log_error("Controller service with name: '{}' is not an Azure Storage credentials service", service_name);
    return std::make_tuple(GetCredentialsFromControllerResult::CONTROLLER_NAME_INVALID, std::nullopt);
  }

  return std::make_tuple(GetCredentialsFromControllerResult::OK, azure_credentials_service->getCredentials());
}

}  // namespace org::apache::nifi::minifi::azure::processors
