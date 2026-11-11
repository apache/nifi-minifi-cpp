// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "../controllerservices/GCPCredentialsControllerService.h"
#include "api/core/ProcessorImpl.h"
#include "core/PropertyDefinitionBuilder.h"
#include "google/cloud/credentials.h"
// google-cloud-cpp uses absl::visit, which is deprecated in newer abseil versions
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "google/cloud/storage/client.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "google/cloud/storage/retry_policy.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "minifi-cpp/core/PropertyValidator.h"

namespace org::apache::nifi::minifi::extensions::gcp {
class GCSProcessor : public api::core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  EXTENSIONAPI static constexpr auto GCPCredentials = core::PropertyDefinitionBuilder<>::createProperty("GCP Credentials Provider Service")
      .withDescription("The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService.")
      .isRequired(true)
      .withAllowedTypes<GCPCredentialsControllerService>()
      .build();
  EXTENSIONAPI static constexpr auto NumberOfRetries = core::PropertyDefinitionBuilder<>::createProperty("Number of retries")
      .withDescription("How many retry attempts should be made before routing to the failure relationship.")
      .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
      .withDefaultValue("6")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto EndpointOverrideURL = core::PropertyDefinitionBuilder<>::createProperty("Endpoint Override URL")
      .withDescription("Overrides the default Google Cloud Storage endpoints")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyConfigurationService = core::PropertyDefinitionBuilder<>::createProperty("Proxy Configuration Service")
      .withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests.")
      .withAllowedType<MINIFI_PROXY_CONFIGURATION_SERVICE_INTERFACE_PROPERTY_TYPE>()
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      GCPCredentials,
      NumberOfRetries,
      EndpointOverrideURL,
      ProxyConfigurationService
  });

 protected:
  minifi_status onScheduleImpl(api::core::ProcessContext& context) override;

  virtual google::cloud::storage::Client getClient() const;
  static std::shared_ptr<google::cloud::Credentials> getCredentials(const api::core::ProcessContext& context);

  std::optional<std::string> endpoint_url_;
  std::shared_ptr<google::cloud::Credentials> gcp_credentials_;
  std::optional<google::cloud::ProxyConfig> proxy_;
  google::cloud::storage::RetryPolicyOption::Type retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(6);
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
