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
#include <filesystem>
#include <string>
#include <memory>

#include "core/controller/ControllerService.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"

#include "google/cloud/storage/oauth2/credentials.h"

namespace org::apache::nifi::minifi::extensions::gcp {

SMART_ENUM(CredentialsLocation,
    (USE_DEFAULT_CREDENTIALS, "Google Application Default Credentials"),
    (USE_COMPUTE_ENGINE_CREDENTIALS, "Use Compute Engine Credentials"),
    (USE_JSON_FILE, "Service Account JSON File"),
    (USE_JSON_CONTENTS, "Service Account JSON"),
    (USE_ANONYMOUS_CREDENTIALS, "Use Anonymous credentials"));

class GCPCredentialsControllerService : public core::controller::ControllerService {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Google Cloud Platform Credentials Controller Service";

  EXTENSIONAPI static constexpr auto CredentialsLoc = core::PropertyDefinitionBuilder<CredentialsLocation::length>::createProperty("Credentials Location")
      .withDescription("The location of the credentials.")
      .withAllowedValues(CredentialsLocation::values)
      .withDefaultValue(toStringView(CredentialsLocation::USE_DEFAULT_CREDENTIALS))
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto JsonFilePath = core::PropertyDefinitionBuilder<>::createProperty("Service Account JSON File")
      .withDescription("Path to a file containing a Service Account key file in JSON format.")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto JsonContents = core::PropertyDefinitionBuilder<>::createProperty("Service Account JSON")
      .withDescription("The raw JSON containing a Service Account keyfile.")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 3>{
      CredentialsLoc,
      JsonFilePath,
      JsonContents
  };


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using ControllerService::ControllerService;

  void initialize() override;

  void yield() override {
  }

  bool isWorkAvailable() override {
    return false;
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;

  [[nodiscard]] const auto& getCredentials() const { return credentials_; }

 protected:
  [[nodiscard]] std::shared_ptr<google::cloud::storage::oauth2::Credentials> createDefaultCredentials() const;
  [[nodiscard]] std::shared_ptr<google::cloud::storage::oauth2::Credentials> createCredentialsFromJsonPath() const;
  [[nodiscard]] std::shared_ptr<google::cloud::storage::oauth2::Credentials> createCredentialsFromJsonContents() const;


  std::shared_ptr<google::cloud::storage::oauth2::Credentials> credentials_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GCPCredentialsControllerService>::getLogger(uuid_);
};
}  // namespace org::apache::nifi::minifi::extensions::gcp
