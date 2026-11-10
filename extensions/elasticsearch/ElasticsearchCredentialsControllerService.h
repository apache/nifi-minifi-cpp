// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


#pragma once

#include <string>
#include <utility>
#include <memory>

#include "http/HTTPClient.h"
#include "core/controller/ControllerServiceBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class ElasticsearchCredentialsControllerService : public core::controller::ControllerServiceBase, public core::controller::ControllerServiceHandle {
 public:
  EXTENSIONAPI static constexpr const char* Description = "Elasticsearch/Opensearch Credentials Controller Service";

  EXTENSIONAPI static constexpr auto Username = core::PropertyDefinitionBuilder<>::createProperty("Username")
      .withDescription("The username for basic authentication")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Password = core::PropertyDefinitionBuilder<>::createProperty("Password")
      .withDescription("The password for basic authentication")
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto ApiKey = core::PropertyDefinitionBuilder<>::createProperty("API Key")
      .withDescription("The API Key to use")
      .isSensitive(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
        Username,
        Password,
        ApiKey
  });


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  using ControllerServiceBase::ControllerServiceBase;

  void initialize() override;

  void onEnable() override;

  [[nodiscard]] ControllerServiceHandle* getControllerServiceHandle() override {return this;}

  void authenticateClient(http::HTTPClient& client);

 private:
  std::optional<std::pair<std::string, std::string>> username_password_;
  std::optional<std::string> api_key_;
};
}  //  namespace org::apache::nifi::minifi::extensions::elasticsearch
