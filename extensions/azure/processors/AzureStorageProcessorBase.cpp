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

#include "core/ProcessContext.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace org::apache::nifi::minifi::azure::processors {

std::tuple<AzureStorageProcessorBase::GetCredentialsFromControllerResult, std::optional<storage::AzureStorageCredentials>> AzureStorageProcessorBase::getCredentialsFromControllerService(
    core::ProcessContext &context) const {
  std::string service_name;
  if (!context.getProperty(AzureStorageCredentialsService.getName(), service_name) || service_name.empty()) {
    return std::make_tuple(GetCredentialsFromControllerResult::CONTROLLER_NAME_EMPTY, std::nullopt);
  }

  std::shared_ptr<core::controller::ControllerService> service = context.getControllerService(service_name);
  if (nullptr == service) {
    logger_->log_error("Azure Storage credentials service with name: '%s' could not be found", service_name);
    return std::make_tuple(GetCredentialsFromControllerResult::CONTROLLER_NAME_INVALID, std::nullopt);
  }

  auto azure_credentials_service = std::dynamic_pointer_cast<minifi::azure::controllers::AzureStorageCredentialsService>(service);
  if (!azure_credentials_service) {
    logger_->log_error("Controller service with name: '%s' is not an Azure Storage credentials service", service_name);
    return std::make_tuple(GetCredentialsFromControllerResult::CONTROLLER_NAME_INVALID, std::nullopt);
  }

  return std::make_tuple(GetCredentialsFromControllerResult::OK, azure_credentials_service->getCredentials());
}

}  // namespace org::apache::nifi::minifi::azure::processors
