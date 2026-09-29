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

#include "AWSCredentialsService.h"

#include <utility>

#include "AWSCredentialsProviderFactory.h"
#include "core/Resource.h"
#include "utils/expected.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::aws::controllers {

void AWSCredentialsService::initialize() {
  setSupportedProperties(Properties);
}

void AWSCredentialsService::onEnable() {
  AWSCredentialsProviderSettings settings;
  if (const auto access_key = getProperty(AccessKey.name)) {
    settings.access_key = *access_key;
  }
  if (const auto secret_key = getProperty(SecretKey.name)) {
    settings.secret_key = *secret_key;
  }
  if (const auto credentials_file = getProperty(CredentialsFile.name)) {
    settings.credentials_file = *credentials_file;
  }
  if (const auto profile_name = getProperty(ProfileName.name)) {
    settings.profile_name = *profile_name;
  }
  if (const auto sso_profile_name = getProperty(SSOProfileName.name)) {
    settings.sso_profile_name = *sso_profile_name;
  }
  if (const auto use_default_credentials = getProperty(UseDefaultCredentials.name) | minifi::utils::andThen(parsing::parseBool); use_default_credentials && *use_default_credentials) {
    settings.credential_configuration_strategy = CredentialConfigurationStrategyOption::DefaultCredentials;
  }

  if (const auto credential_configuration_strategy = getProperty(CredentialConfigurationStrategy.name)) {
    if (auto strategy = magic_enum::enum_cast<CredentialConfigurationStrategyOption>(*credential_configuration_strategy, magic_enum::case_insensitive)) {
      if (settings.credential_configuration_strategy) {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Both {0} and {1} properties are set! Only one of them should be set, please prefer using the {0} property",
                                                                CredentialConfigurationStrategy.name, UseDefaultCredentials.name));
      }
      settings.credential_configuration_strategy = strategy;
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Invalid value for {} property: {}", CredentialConfigurationStrategy.name, *credential_configuration_strategy));
    }
  }

  std::lock_guard<std::mutex> lock(credentials_provider_mutex_);
  credentials_provider_settings_ = std::move(settings);
  credentials_provider_.reset();
}

std::shared_ptr<Aws::Auth::AWSCredentialsProvider> AWSCredentialsService::getAWSCredentialsProvider() {
  std::lock_guard<std::mutex> lock(credentials_provider_mutex_);
  if (!credentials_provider_) {
    credentials_provider_ = createAWSCredentialsProvider(credentials_provider_settings_, *logger_);
  }
  return credentials_provider_;
}

REGISTER_RESOURCE(AWSCredentialsService, ControllerService);

}  // namespace org::apache::nifi::minifi::aws::controllers
