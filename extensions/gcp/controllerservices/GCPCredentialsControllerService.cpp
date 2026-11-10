// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements


#include "GCPCredentialsControllerService.h"

#include "google/cloud/storage/client.h"

namespace org::apache::nifi::minifi::extensions::gcp {

namespace {
// TODO(MINIFICPP-2763) use utils::file::get_content instead
std::string get_content(const std::filesystem::path& file_name) {
  std::ifstream file(file_name, std::ifstream::binary);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return content;
}
}

std::shared_ptr<google::cloud::Credentials> GCPCredentialsControllerService::createCredentialsFromJsonPath(api::core::ControllerServiceContext& ctx) const {
  const auto json_path = ctx.getProperty(JsonFilePath.name);
  if (!json_path) {
    logger_->log_error("Missing or invalid {}", JsonFilePath.name);
    return nullptr;
  }

  if (std::error_code ec; !std::filesystem::exists(*json_path, ec) || ec) {
    logger_->log_error("JSON file for GCP credentials '{}' does not exist", *json_path);
    return nullptr;
  }

  return google::cloud::MakeServiceAccountCredentials(get_content(*json_path));
}

std::shared_ptr<google::cloud::Credentials> GCPCredentialsControllerService::createCredentialsFromJsonContents(api::core::ControllerServiceContext& ctx) const {
  auto json_contents = ctx.getProperty(JsonContents.name);
  if (!json_contents) {
    logger_->log_error("Missing or invalid {}", JsonContents.name);
    return nullptr;
  }

  return google::cloud::MakeServiceAccountCredentials(*json_contents);
}

minifi_status GCPCredentialsControllerService::enableImpl(api::core::ControllerServiceContext& ctx) {
  std::optional<CredentialsLocation> credentials_location;
  if (const auto value = ctx.getProperty(CredentialsLoc.name)) {
    credentials_location = magic_enum::enum_cast<CredentialsLocation>(*value);
  }
  if (!credentials_location) {
    logger_->log_error("Invalid Credentials Location, defaulting to {}", magic_enum::enum_name(CredentialsLocation::USE_DEFAULT_CREDENTIALS));
    credentials_location = CredentialsLocation::USE_DEFAULT_CREDENTIALS;
  }
  if (*credentials_location == CredentialsLocation::USE_DEFAULT_CREDENTIALS) {
    credentials_ = google::cloud::MakeGoogleDefaultCredentials();
  } else if (*credentials_location == CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS) {
    credentials_ = google::cloud::MakeComputeEngineCredentials();
  } else if (*credentials_location == CredentialsLocation::USE_JSON_FILE) {
    credentials_ = createCredentialsFromJsonPath(ctx);
  } else if (*credentials_location == CredentialsLocation::USE_JSON_CONTENTS) {
    credentials_ = createCredentialsFromJsonContents(ctx);
  } else if (*credentials_location == CredentialsLocation::USE_ANONYMOUS_CREDENTIALS) {
    credentials_ = google::cloud::MakeInsecureCredentials();
  }
  if (!credentials_)
    logger_->log_error("Couldn't create valid credentials");
  return MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
