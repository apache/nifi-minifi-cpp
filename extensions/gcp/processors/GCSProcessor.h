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

#include <string>
#include <memory>
#include <utility>
#include <optional>

#include "../controllerservices/GCPCredentialsControllerService.h"
#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/retry_policy.h"

namespace org::apache::nifi::minifi::extensions::gcp {
class GCSProcessor : public core::ProcessorImpl {
 public:
  GCSProcessor(std::string_view name, const minifi::utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
      : core::ProcessorImpl(name, uuid),
   logger_(std::move(logger)) {
  }

  EXTENSIONAPI static constexpr auto GCPCredentials = core::PropertyDefinitionBuilder<>::createProperty("GCP Credentials Provider Service")
      .withDescription("The Controller Service used to obtain Google Cloud Platform credentials. Should be the name of a GCPCredentialsControllerService.")
      .isRequired(true)
      .withAllowedTypes<GCPCredentialsControllerService>()
      .build();
  EXTENSIONAPI static constexpr auto NumberOfRetries = core::PropertyDefinitionBuilder<>::createProperty("Number of retries")
      .withDescription("How many retry attempts should be made before routing to the failure relationship.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("6")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto EndpointOverrideURL = core::PropertyDefinitionBuilder<>::createProperty("Endpoint Override URL")
      .withDescription("Overrides the default Google Cloud Storage endpoints")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      GCPCredentials,
      NumberOfRetries,
      EndpointOverrideURL
  });


  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  virtual google::cloud::storage::Client getClient() const;

  std::optional<std::string> endpoint_url_;
  std::shared_ptr<google::cloud::storage::oauth2::Credentials> gcp_credentials_;
  google::cloud::storage::RetryPolicyOption::Type retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(6);
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
