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

#include "core/PropertyBuilder.h"
#include "core/Resource.h"
#include "google/cloud/storage/client.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

const core::Property GCPCredentialsControllerService::CredentialsLoc(
    core::PropertyBuilder::createProperty("Credentials Location")
        ->withDescription("The location of the credentials.")
        ->withAllowableValues(CredentialsLocation::values())
        ->withDefaultValue(toString(CredentialsLocation::USE_DEFAULT_CREDENTIALS))
        ->isRequired(true)
        ->build());

const core::Property GCPCredentialsControllerService::JsonFilePath(
    core::PropertyBuilder::createProperty("Service Account JSON File")
        ->withDescription("Path to a file containing a Service Account key file in JSON format.")
        ->isRequired(false)
        ->build());

const core::Property GCPCredentialsControllerService::JsonContents(
    core::PropertyBuilder::createProperty("Service Account JSON")
        ->withDescription("The raw JSON containing a Service Account keyfile.")
        ->isRequired(false)
        ->build());

void GCPCredentialsControllerService::initialize() {
  setSupportedProperties(properties());
}

std::shared_ptr<gcs::oauth2::Credentials> GCPCredentialsControllerService::createDefaultCredentials() const {
  auto default_credentials = gcs::oauth2::CreateServiceAccountCredentialsFromDefaultPaths();
  if (!default_credentials.ok()) {
    logger_->log_error(default_credentials.status().message().c_str());
    return nullptr;
  }
  return *default_credentials;
}

std::shared_ptr<gcs::oauth2::Credentials> GCPCredentialsControllerService::createCredentialsFromJsonPath() const {
  std::string json_path;
  if (!getProperty(JsonFilePath.getName(), json_path)) {
    logger_->log_error("Missing or invalid %s", JsonFilePath.getName());
    return nullptr;
  }

  auto json_path_credentials = gcs::oauth2::CreateServiceAccountCredentialsFromJsonFilePath(json_path);
  if (!json_path_credentials.ok()) {
    logger_->log_error(json_path_credentials.status().message().c_str());
    return nullptr;
  }
  return *json_path_credentials;
}

std::shared_ptr<gcs::oauth2::Credentials> GCPCredentialsControllerService::createCredentialsFromJsonContents() const {
  std::string json_contents;
  if (!getProperty(JsonContents.getName(), json_contents)) {
    logger_->log_error("Missing or invalid %s", JsonContents.getName());
    return nullptr;
  }

  auto json_path_credentials = gcs::oauth2::CreateServiceAccountCredentialsFromJsonContents(json_contents);
  if (!json_path_credentials.ok()) {
    logger_->log_error(json_path_credentials.status().message().c_str());
    return nullptr;
  }
  return *json_path_credentials;
}

void GCPCredentialsControllerService::onEnable() {
  CredentialsLocation credentials_location;
  if (!getProperty(CredentialsLoc.getName(), credentials_location)) {
    logger_->log_error("Invalid Credentials Location, defaulting to %s", toString(CredentialsLocation::USE_DEFAULT_CREDENTIALS));
    credentials_location = CredentialsLocation::USE_DEFAULT_CREDENTIALS;
  }
  if (credentials_location == CredentialsLocation::USE_DEFAULT_CREDENTIALS) {
    credentials_ = createDefaultCredentials();
  } else if (credentials_location == CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS) {
    credentials_ = gcs::oauth2::CreateComputeEngineCredentials();
  } else if (credentials_location == CredentialsLocation::USE_JSON_FILE) {
    credentials_ = createCredentialsFromJsonPath();
  } else if (credentials_location == CredentialsLocation::USE_JSON_CONTENTS) {
    credentials_ = createCredentialsFromJsonContents();
  } else if (credentials_location == CredentialsLocation::USE_ANONYMOUS_CREDENTIALS) {
    credentials_ = gcs::oauth2::CreateAnonymousCredentials();
  }
  if (!credentials_)
    logger_->log_error("Couldn't create valid credentials");
}

REGISTER_RESOURCE(GCPCredentialsControllerService, ControllerService);
}  // namespace org::apache::nifi::minifi::extensions::gcp

