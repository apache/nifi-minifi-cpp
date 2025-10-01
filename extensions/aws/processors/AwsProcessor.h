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

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "aws/core/auth/AWSCredentialsProvider.h"
#include "AWSCredentialsProvider.h"
#include "utils/ProxyOptions.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "core/ProcessorImpl.h"
#include "minifi-cpp/controllers/ProxyConfigurationServiceInterface.h"


namespace org::apache::nifi::minifi::aws::processors {

namespace region {
inline constexpr std::string_view AF_SOUTH_1 = "af-south-1";
inline constexpr std::string_view AP_EAST_1 = "ap-east-1";
inline constexpr std::string_view AP_NORTHEAST_1 = "ap-northeast-1";
inline constexpr std::string_view AP_NORTHEAST_2 = "ap-northeast-2";
inline constexpr std::string_view AP_NORTHEAST_3 = "ap-northeast-3";
inline constexpr std::string_view AP_SOUTH_1 = "ap-south-1";
inline constexpr std::string_view AP_SOUTH_2 = "ap-south-2";
inline constexpr std::string_view AP_SOUTHEAST_1 = "ap-southeast-1";
inline constexpr std::string_view AP_SOUTHEAST_2 = "ap-southeast-2";
inline constexpr std::string_view AP_SOUTHEAST_3 = "ap-southeast-3";
inline constexpr std::string_view AP_SOUTHEAST_4 = "ap-southeast-4";
inline constexpr std::string_view AP_SOUTHEAST_5 = "ap-southeast-5";
inline constexpr std::string_view AP_SOUTHEAST_7 = "ap-southeast-7";
inline constexpr std::string_view CA_CENTRAL_1 = "ca-central-1";
inline constexpr std::string_view CA_WEST_1 = "ca-west-1";
inline constexpr std::string_view CN_NORTH_1 = "cn-north-1";
inline constexpr std::string_view CN_NORTHWEST_1 = "cn-northwest-1";
inline constexpr std::string_view EU_CENTRAL_1 = "eu-central-1";
inline constexpr std::string_view EU_CENTRAL_2 = "eu-central-2";
inline constexpr std::string_view EU_ISOE_WEST_1 = "eu-isoe-west-1";
inline constexpr std::string_view EU_NORTH_1 = "eu-north-1";
inline constexpr std::string_view EU_SOUTH_1 = "eu-south-1";
inline constexpr std::string_view EU_SOUTH_2 = "eu-south-2";
inline constexpr std::string_view EU_WEST_1 = "eu-west-1";
inline constexpr std::string_view EU_WEST_2 = "eu-west-2";
inline constexpr std::string_view EU_WEST_3 = "eu-west-3";
inline constexpr std::string_view IL_CENTRAL_1 = "il-central-1";
inline constexpr std::string_view ME_CENTRAL_1 = "me-central-1";
inline constexpr std::string_view ME_SOUTH_1 = "me-south-1";
inline constexpr std::string_view MX_CENTRAL_1 = "mx-central-1";
inline constexpr std::string_view SA_EAST_1 = "sa-east-1";
inline constexpr std::string_view US_EAST_1 = "us-east-1";
inline constexpr std::string_view US_EAST_2 = "us-east-2";
inline constexpr std::string_view US_GOV_EAST_1 = "us-gov-east-1";
inline constexpr std::string_view US_GOV_WEST_1 = "us-gov-west-1";
inline constexpr std::string_view US_ISO_EAST_1 = "us-iso-east-1";
inline constexpr std::string_view US_ISO_WEST_1 = "us-iso-west-1";
inline constexpr std::string_view US_ISOB_EAST_1 = "us-isob-east-1";
inline constexpr std::string_view US_ISOF_EAST_1 = "us-isof-east-1";
inline constexpr std::string_view US_ISOF_SOUTH_1 = "us-isof-south-1";
inline constexpr std::string_view US_WEST_1 = "us-west-1";
inline constexpr std::string_view US_WEST_2 = "us-west-2";

inline constexpr auto REGIONS = std::array{
  AF_SOUTH_1, AP_EAST_1, AP_NORTHEAST_1, AP_NORTHEAST_2, AP_NORTHEAST_3, AP_SOUTH_1, AP_SOUTH_2, AP_SOUTHEAST_1, AP_SOUTHEAST_2, AP_SOUTHEAST_3, AP_SOUTHEAST_4, AP_SOUTHEAST_5, AP_SOUTHEAST_7,
  CA_CENTRAL_1, CA_WEST_1, CN_NORTH_1, CN_NORTHWEST_1, EU_CENTRAL_1, EU_CENTRAL_2, EU_ISOE_WEST_1, EU_NORTH_1, EU_SOUTH_1, EU_SOUTH_2, EU_WEST_1, EU_WEST_2, EU_WEST_3, IL_CENTRAL_1, ME_CENTRAL_1,
  ME_SOUTH_1, MX_CENTRAL_1, SA_EAST_1, US_EAST_1, US_EAST_2, US_GOV_EAST_1, US_GOV_WEST_1, US_ISO_EAST_1, US_ISO_WEST_1, US_ISOB_EAST_1, US_ISOF_EAST_1, US_ISOF_SOUTH_1, US_WEST_1, US_WEST_2
};
}  // namespace region

struct CommonProperties {
  Aws::Auth::AWSCredentials credentials;
  aws::ProxyOptions proxy;
  std::string endpoint_override_url;
};

class AwsProcessor : public core::ProcessorImpl {  // NOLINT(cppcoreguidelines-special-member-functions)
 public:
  EXTENSIONAPI static constexpr auto AccessKey = core::PropertyDefinitionBuilder<>::createProperty("Access Key")
      .withDescription("AWS account access key")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto SecretKey = core::PropertyDefinitionBuilder<>::createProperty("Secret Key")
      .withDescription("AWS account secret key")
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto CredentialsFile = core::PropertyDefinitionBuilder<>::createProperty("Credentials File")
      .withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
      .build();
  EXTENSIONAPI static constexpr auto AWSCredentialsProviderService = core::PropertyDefinitionBuilder<>::createProperty("AWS Credentials Provider service")
      .withDescription("The name of the AWS Credentials Provider controller service that is used to obtain AWS credentials.")
      .build();
  EXTENSIONAPI static constexpr auto Region = core::PropertyDefinitionBuilder<region::REGIONS.size()>::createProperty("Region")
      .isRequired(true)
      .withDefaultValue(region::US_WEST_2)
      .withAllowedValues(region::REGIONS)
      .withDescription("AWS Region")
      .build();
  EXTENSIONAPI static constexpr auto CommunicationsTimeout = core::PropertyDefinitionBuilder<>::createProperty("Communications Timeout")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::TIME_PERIOD_VALIDATOR)
      .withDefaultValue("30 sec")
      .withDescription("Sets the timeout of the communication between the AWS server and the client")
      .build();
  EXTENSIONAPI static constexpr auto EndpointOverrideURL = core::PropertyDefinitionBuilder<>::createProperty("Endpoint Override URL")
      .withDescription("Endpoint URL to use instead of the AWS default including scheme, host, "
          "port, and path. The AWS libraries select an endpoint URL based on the AWS "
          "region, but this property overrides the selected endpoint URL, allowing use "
          "with other S3-compatible endpoints.")
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyHost = core::PropertyDefinitionBuilder<>::createProperty("Proxy Host")
      .withDescription("Proxy host name or IP")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyPort = core::PropertyDefinitionBuilder<>::createProperty("Proxy Port")
      .withDescription("The port number of the proxy host")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyUsername = core::PropertyDefinitionBuilder<>::createProperty("Proxy Username")
      .withDescription("Username to set when authenticating against proxy")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyPassword = core::PropertyDefinitionBuilder<>::createProperty("Proxy Password")
      .withDescription("Password to set when authenticating against proxy")
      .supportsExpressionLanguage(true)
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto ProxyConfigurationService = core::PropertyDefinitionBuilder<>::createProperty("Proxy Configuration Service")
      .withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. When used, "
          "this will override any values specified for Proxy Host, Proxy Port, Proxy Username, and Proxy Password properties.")
      .withAllowedTypes<minifi::controllers::ProxyConfigurationServiceInterface>()
      .build();
  EXTENSIONAPI static constexpr auto UseDefaultCredentials = core::PropertyDefinitionBuilder<>::createProperty("Use Default Credentials")
      .withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      AccessKey,
      SecretKey,
      CredentialsFile,
      AWSCredentialsProviderService,
      Region,
      CommunicationsTimeout,
      EndpointOverrideURL,
      ProxyHost,
      ProxyPort,
      ProxyUsername,
      ProxyPassword,
      ProxyConfigurationService,
      UseDefaultCredentials
  });

  using ProcessorImpl::ProcessorImpl;
  ~AwsProcessor() override = default;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  std::optional<Aws::Auth::AWSCredentials> getAWSCredentialsFromControllerService(core::ProcessContext& context) const;
  std::optional<Aws::Auth::AWSCredentials> getAWSCredentials(core::ProcessContext& context, const core::FlowFile* flow_file);
  aws::ProxyOptions getProxy(core::ProcessContext& context, const core::FlowFile* const flow_file);
  std::optional<CommonProperties> getCommonELSupportedProperties(core::ProcessContext& context, const core::FlowFile* flow_file);

  std::optional<Aws::Client::ClientConfiguration> client_config_;
};

}  // namespace org::apache::nifi::minifi::aws::processors
