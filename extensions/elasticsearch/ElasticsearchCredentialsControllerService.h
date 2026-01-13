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
#include <utility>
#include <memory>

#include "http/HTTPClient.h"
#include "core/controller/ControllerServiceBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class ElasticsearchCredentialsControllerService : public core::controller::ControllerServiceBase, public core::controller::ControllerServiceInterface {
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

  using ControllerServiceBase::ControllerServiceBase;

  void initialize() override;

  void onEnable() override;

  ControllerServiceInterface* getControllerServiceInterface() override {return this;}

  void authenticateClient(http::HTTPClient& client);

 private:
  std::optional<std::pair<std::string, std::string>> username_password_;
  std::optional<std::string> api_key_;
};
}  //  namespace org::apache::nifi::minifi::extensions::elasticsearch
