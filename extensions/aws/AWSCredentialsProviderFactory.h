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

#include "aws/core/auth/AWSCredentialsProvider.h"
#include "magic_enum/magic_enum.hpp"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::aws {

enum class CredentialConfigurationStrategyOption {
  FromProperties,
  CredentialsFile,
  DefaultCredentials,
  EnvironmentVariables,
  ProfileCredentials,
  SSOCredentials,
  STSAssumeRoleWebIdentityCredentials,
  InstanceProfileCredentials,
  AnonymousCredentials,
};

}  // namespace org::apache::nifi::minifi::aws

namespace magic_enum::customize {
using CredentialConfigurationStrategyOption = org::apache::nifi::minifi::aws::CredentialConfigurationStrategyOption;

template<>
constexpr customize_t enum_name<CredentialConfigurationStrategyOption>(CredentialConfigurationStrategyOption value) noexcept {
  switch (value) {
    case CredentialConfigurationStrategyOption::FromProperties:
      return "From Properties";
    case CredentialConfigurationStrategyOption::CredentialsFile:
      return "Credentials File";
    case CredentialConfigurationStrategyOption::DefaultCredentials:
      return "Default Credentials";
    case CredentialConfigurationStrategyOption::EnvironmentVariables:
      return "Environment Variables";
    case CredentialConfigurationStrategyOption::ProfileCredentials:
      return "Profile Credentials";
    case CredentialConfigurationStrategyOption::SSOCredentials:
      return "SSO Credentials";
    case CredentialConfigurationStrategyOption::STSAssumeRoleWebIdentityCredentials:
      return "STS Web Identity Credentials";
    case CredentialConfigurationStrategyOption::InstanceProfileCredentials:
      return "Instance Profile Credentials";
    case CredentialConfigurationStrategyOption::AnonymousCredentials:
      return "Anonymous Credentials";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::aws {

struct AWSCredentialsProviderSettings {
  std::optional<CredentialConfigurationStrategyOption> credential_configuration_strategy{};
  std::string access_key{};
  std::string secret_key{};
  std::string credentials_file{};
  std::string profile_name{};
  std::string sso_profile_name{};
};

[[nodiscard]] std::shared_ptr<Aws::Auth::AWSCredentialsProvider> createAWSCredentialsProvider(const AWSCredentialsProviderSettings& settings,
    core::logging::Logger& logger);

}  // namespace org::apache::nifi::minifi::aws
