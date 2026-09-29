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

#include "AWSCredentialsProviderFactory.h"

#include <filesystem>
#include <string>
#include <string_view>

#include "aws/core/auth/AWSCredentialsProviderChain.h"
#include "aws/core/auth/ProfileCredentialsProvider.h"
#include "aws/core/auth/SSOCredentialsProvider.h"
#include "aws/core/auth/STSCredentialsProvider.h"
#include "minifi-cpp/properties/Properties.h"

namespace org::apache::nifi::minifi::aws {

namespace {

std::shared_ptr<Aws::Auth::AWSCredentialsProvider> probeProvider(const std::shared_ptr<Aws::Auth::AWSCredentialsProvider>& provider,
    std::string_view credentials_source, core::logging::Logger& logger) {
  if (provider->GetAWSCredentials().IsEmpty()) {
    logger.log_error("No AWS credentials were found in {}.", credentials_source);
    return nullptr;
  }
  logger.log_debug("Using AWS credentials from {}.", credentials_source);
  return provider;
}

std::shared_ptr<Aws::Auth::AWSCredentialsProvider> createProviderFromCredentialsFile(const std::string& credentials_file,
    core::logging::Logger& logger) {
  if (!std::filesystem::exists(credentials_file)) {
    logger.log_error("The specified credentials file does not exist!");
    return nullptr;
  }

  auto properties = minifi::Properties::create();
  properties->loadConfigureFile(credentials_file.c_str());
  std::string access_key;
  std::string secret_key;
  if (properties->getString("accessKey", access_key) && !access_key.empty() && properties->getString("secretKey", secret_key) &&
      !secret_key.empty()) {
    logger.log_debug("Using AWS credentials from credentials file.");
    return std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(access_key, secret_key);
  }
  logger.log_error("No AWS credentials were found in the credentials file.");
  return nullptr;
}

}  // namespace

std::shared_ptr<Aws::Auth::AWSCredentialsProvider> createAWSCredentialsProvider(const AWSCredentialsProviderSettings& settings,
    core::logging::Logger& logger) {
  if (!settings.credential_configuration_strategy) {
    if (!settings.access_key.empty() && !settings.secret_key.empty()) {
      logger.log_debug("Using access key and secret key as AWS credentials.");
      return std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(settings.access_key, settings.secret_key);
    }

    if (!settings.credentials_file.empty()) {
      if (auto credentials_file_provider = createProviderFromCredentialsFile(settings.credentials_file, logger)) {
        return credentials_file_provider;
      }
    }

    logger.log_error("No Credential Configuration Strategy was set and no AWS credentials were set.");
    return nullptr;
  }

  const auto strategy = *settings.credential_configuration_strategy;
  if (strategy == CredentialConfigurationStrategyOption::FromProperties) {
    if (settings.access_key.empty() || settings.secret_key.empty()) {
      logger.log_error("Access key or secret key is missing for FromProperties strategy.");
      return nullptr;
    }
    logger.log_debug("Using access key and secret key as AWS credentials.");
    return std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(settings.access_key, settings.secret_key);
  } else if (strategy == CredentialConfigurationStrategyOption::CredentialsFile) {
    if (settings.credentials_file.empty()) {
      logger.log_error("Credentials file path is empty for CredentialsFile strategy.");
      return nullptr;
    }
    return createProviderFromCredentialsFile(settings.credentials_file, logger);
  } else if (strategy == CredentialConfigurationStrategyOption::DefaultCredentials) {
    logger.log_debug("Trying to use default AWS credentials provider chain.");
    return probeProvider(std::make_shared<Aws::Auth::DefaultAWSCredentialsProviderChain>(), "the default AWS credentials provider chain", logger);
  } else if (strategy == CredentialConfigurationStrategyOption::EnvironmentVariables) {
    return probeProvider(std::make_shared<Aws::Auth::EnvironmentAWSCredentialsProvider>(), "environment variables", logger);
  } else if (strategy == CredentialConfigurationStrategyOption::ProfileCredentials) {
    auto profile_provider = settings.profile_name.empty()
        ? std::make_shared<Aws::Auth::ProfileCredentialsProvider>()
        : std::make_shared<Aws::Auth::ProfileCredentialsProvider>(settings.profile_name.c_str());
    return probeProvider(profile_provider, "profile", logger);
  } else if (strategy == CredentialConfigurationStrategyOption::InstanceProfileCredentials) {
    return probeProvider(std::make_shared<Aws::Auth::InstanceProfileCredentialsProvider>(), "instance profile", logger);
  } else if (strategy == CredentialConfigurationStrategyOption::AnonymousCredentials) {
    logger.log_debug("Using anonymous AWS credentials.");
    return std::make_shared<Aws::Auth::AnonymousAWSCredentialsProvider>();
  } else if (strategy == CredentialConfigurationStrategyOption::SSOCredentials) {
    auto sso_provider = settings.sso_profile_name.empty()
        ? std::make_shared<Aws::Auth::SSOCredentialsProvider>()
        : std::make_shared<Aws::Auth::SSOCredentialsProvider>(settings.sso_profile_name);
    return probeProvider(sso_provider, "SSO", logger);
  } else if (strategy == CredentialConfigurationStrategyOption::STSAssumeRoleWebIdentityCredentials) {
    return probeProvider(std::make_shared<Aws::Auth::STSAssumeRoleWebIdentityCredentialsProvider>(), "STS Assume Role Web Identity", logger);
  }

  logger.log_error("Unsupported Credential Configuration Strategy.");
  return nullptr;
}

}  // namespace org::apache::nifi::minifi::aws
