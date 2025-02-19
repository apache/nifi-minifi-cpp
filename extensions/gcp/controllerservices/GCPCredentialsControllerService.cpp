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


#include "GCPCredentialsControllerService.h"

#include "core/Resource.h"
#include "google/cloud/storage/client.h"
#include "utils/ProcessorConfigUtils.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

void GCPCredentialsControllerService::initialize() {
  setSupportedProperties(Properties);
}

std::shared_ptr<gcs::oauth2::Credentials> GCPCredentialsControllerService::createDefaultCredentials() const {
  auto default_credentials = gcs::oauth2::CreateServiceAccountCredentialsFromDefaultPaths();
  if (!default_credentials.ok()) {
    logger_->log_error("{}", default_credentials.status().message());
    return nullptr;
  }
  return *default_credentials;
}

std::shared_ptr<gcs::oauth2::Credentials> GCPCredentialsControllerService::createCredentialsFromJsonPath() const {
  const auto json_path = getProperty(JsonFilePath.name);
  if (!json_path) {
    logger_->log_error("Missing or invalid {}", JsonFilePath.name);
    return nullptr;
  }

  auto json_path_credentials = gcs::oauth2::CreateServiceAccountCredentialsFromJsonFilePath(*json_path);
  if (!json_path_credentials.ok()) {
    logger_->log_error("{}", json_path_credentials.status().message());
    return nullptr;
  }
  return *json_path_credentials;
}

std::shared_ptr<gcs::oauth2::Credentials> GCPCredentialsControllerService::createCredentialsFromJsonContents() const {
  auto json_contents = getProperty(JsonContents.name);
  if (!json_contents) {
    logger_->log_error("Missing or invalid {}", JsonContents.name);
    return nullptr;
  }

  auto json_path_credentials = gcs::oauth2::CreateServiceAccountCredentialsFromJsonContents(*json_contents);
  if (!json_path_credentials.ok()) {
    logger_->log_error("{}", json_path_credentials.status().message());
    return nullptr;
  }
  return *json_path_credentials;
}

void GCPCredentialsControllerService::onEnable() {
  std::optional<CredentialsLocation> credentials_location;
  if (const auto value = getProperty(CredentialsLoc.name)) {
    credentials_location = magic_enum::enum_cast<CredentialsLocation>(*value);
  }
  if (!credentials_location) {
    logger_->log_error("Invalid Credentials Location, defaulting to {}", magic_enum::enum_name(CredentialsLocation::USE_DEFAULT_CREDENTIALS));
    credentials_location = CredentialsLocation::USE_DEFAULT_CREDENTIALS;
  }
  if (*credentials_location == CredentialsLocation::USE_DEFAULT_CREDENTIALS) {
    credentials_ = createDefaultCredentials();
  } else if (*credentials_location == CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS) {
    credentials_ = gcs::oauth2::CreateComputeEngineCredentials();
  } else if (*credentials_location == CredentialsLocation::USE_JSON_FILE) {
    credentials_ = createCredentialsFromJsonPath();
  } else if (*credentials_location == CredentialsLocation::USE_JSON_CONTENTS) {
    credentials_ = createCredentialsFromJsonContents();
  } else if (*credentials_location == CredentialsLocation::USE_ANONYMOUS_CREDENTIALS) {
    credentials_ = gcs::oauth2::CreateAnonymousCredentials();
  }
  if (!credentials_)
    logger_->log_error("Couldn't create valid credentials");
}

REGISTER_RESOURCE(GCPCredentialsControllerService, ControllerService);
}  // namespace org::apache::nifi::minifi::extensions::gcp
