/**
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

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "aws/core/auth/AWSCredentials.h"

#include "utils/AWSInitializer.h"
#include "core/controller/ControllerServiceBase.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "AWSCredentialsProvider.h"

class AWSCredentialsServiceTestAccessor;

namespace org::apache::nifi::minifi::aws::controllers {

class AWSCredentialsService : public core::controller::ControllerServiceBase {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  EXTENSIONAPI static constexpr const char* Description = "Manages the Amazon Web Services (AWS) credentials for an AWS account. This allows for multiple "
      "AWS credential services to be defined. This also allows for multiple AWS related processors to reference this single "
      "controller service so that AWS credentials can be managed and controlled in a central location.";

  EXTENSIONAPI static constexpr auto UseDefaultCredentials = core::PropertyDefinitionBuilder<>::createProperty("Use Default Credentials")
      .withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto AccessKey = core::PropertyDefinitionBuilder<>::createProperty("Access Key")
      .withDescription("Specifies the AWS Access Key.")
      .build();
  EXTENSIONAPI static constexpr auto SecretKey = core::PropertyDefinitionBuilder<>::createProperty("Secret Key")
      .withDescription("Specifies the AWS Secret Key.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto CredentialsFile = core::PropertyDefinitionBuilder<>::createProperty("Credentials File")
      .withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      UseDefaultCredentials,
      AccessKey,
      SecretKey,
      CredentialsFile
  });


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;

  void initialize() override;

  void onEnable() override;

  std::optional<Aws::Auth::AWSCredentials> getAWSCredentials();

 private:
  friend class ::AWSCredentialsServiceTestAccessor;

  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::optional<Aws::Auth::AWSCredentials> aws_credentials_;
  AWSCredentialsProvider aws_credentials_provider_;
};

}  // namespace org::apache::nifi::minifi::aws::controllers
