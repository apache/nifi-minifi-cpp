// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <optional>
#include <tuple>

#include "controllers/ProxyConfiguration.h"
#include "controllerservices/AzureStorageCredentialsService.h"
#include "core/ProcessorImpl.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "storage/AzureStorageCredentials.h"

namespace org::apache::nifi::minifi::azure::processors {

class AzureStorageProcessorBase : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr auto AzureStorageCredentialsService = core::PropertyDefinitionBuilder<>::createProperty("Azure Storage Credentials Service")
      .withDescription("Name of the Azure Storage Credentials Service used to retrieve the connection string from.")
      .withAllowedTypes<azure::controllers::AzureStorageCredentialsService>()
      .build();
  EXTENSIONAPI static constexpr auto ProxyConfigurationService = core::PropertyDefinitionBuilder<>::createProperty("Proxy Configuration Service")
      .withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests.")
      .withAllowedTypes<minifi::controllers::ProxyConfigurationServiceInterface>()
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({AzureStorageCredentialsService, ProxyConfigurationService});

  using ProcessorImpl::ProcessorImpl;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  enum class GetCredentialsFromControllerResult {
    OK,
    CONTROLLER_NAME_EMPTY,
    CONTROLLER_NAME_INVALID
  };

  std::tuple<GetCredentialsFromControllerResult, std::optional<storage::AzureStorageCredentials>> getCredentialsFromControllerService(core::ProcessContext &context) const;

  minifi::controllers::ProxyConfiguration proxy_configuration_;
};

}  // namespace org::apache::nifi::minifi::azure::processors
