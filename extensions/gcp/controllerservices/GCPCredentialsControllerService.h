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
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"

#include "google/cloud/credentials.h"

namespace org::apache::nifi::minifi::extensions::gcp {
enum class CredentialsLocation {
  USE_DEFAULT_CREDENTIALS,
  USE_COMPUTE_ENGINE_CREDENTIALS,
  USE_JSON_FILE,
  USE_JSON_CONTENTS,
  USE_ANONYMOUS_CREDENTIALS
};
}  // namespace org::apache::nifi::minifi::extensions::gcp

namespace magic_enum::customize {
using CredentialsLocation = org::apache::nifi::minifi::extensions::gcp::CredentialsLocation;

template <>
constexpr customize_t enum_name<CredentialsLocation>(CredentialsLocation value) noexcept {
  switch (value) {
    case CredentialsLocation::USE_DEFAULT_CREDENTIALS:
      return "Google Application Default Credentials";
    case CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS:
      return "Use Compute Engine Credentials";
    case CredentialsLocation::USE_JSON_FILE:
      return "Service Account JSON File";
    case CredentialsLocation::USE_JSON_CONTENTS:
      return "Service Account JSON";
    case CredentialsLocation::USE_ANONYMOUS_CREDENTIALS:
      return "Use Anonymous credentials";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::extensions::gcp {

class GCPCredentialsControllerService : public core::controller::ControllerServiceImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Manages the credentials for Google Cloud Platform. This allows for multiple Google Cloud Platform related processors "
      "to reference this single controller service so that Google Cloud Platform credentials can be managed and controlled in a central location.";

  EXTENSIONAPI static constexpr auto CredentialsLoc = core::PropertyDefinitionBuilder<magic_enum::enum_count<CredentialsLocation>()>::createProperty("Credentials Location")
      .withDescription("The location of the credentials.")
      .withDefaultValue(magic_enum::enum_name(CredentialsLocation::USE_DEFAULT_CREDENTIALS))
      .withAllowedValues(magic_enum::enum_names<CredentialsLocation>())
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto JsonFilePath = core::PropertyDefinitionBuilder<>::createProperty("Service Account JSON File")
      .withDescription("Path to a file containing a Service Account key file in JSON format.")
      .isRequired(false)
      .build();
  EXTENSIONAPI static constexpr auto JsonContents = core::PropertyDefinitionBuilder<>::createProperty("Service Account JSON")
      .withDescription("The raw JSON containing a Service Account keyfile.")
      .isRequired(false)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      CredentialsLoc,
      JsonFilePath,
      JsonContents
  });


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using ControllerServiceImpl::ControllerServiceImpl;

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
  [[nodiscard]] std::shared_ptr<google::cloud::Credentials> createCredentialsFromJsonPath() const;
  [[nodiscard]] std::shared_ptr<google::cloud::Credentials> createCredentialsFromJsonContents() const;


  std::shared_ptr<google::cloud::Credentials> credentials_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GCPCredentialsControllerService>::getLogger(uuid_);
};
}  // namespace org::apache::nifi::minifi::extensions::gcp
