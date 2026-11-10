// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "ElasticsearchCredentialsControllerService.h"


#include <utility>

#include "core/Resource.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

void ElasticsearchCredentialsControllerService::initialize() {
  setSupportedProperties(Properties);
}

void ElasticsearchCredentialsControllerService::onEnable() {
  api_key_ = getProperty(ApiKey.name) | utils::toOptional();
  std::string username = getProperty(Username.name).value_or("");
  std::string password = getProperty(Password.name).value_or("");

  if (!username.empty() && !password.empty())
    username_password_.emplace(std::move(username), std::move(password));
  if (api_key_.has_value() == username_password_.has_value())
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either an API Key or Username and Password must be provided");
}

void ElasticsearchCredentialsControllerService::authenticateClient(http::HTTPClient& client) {
  gsl_Expects(api_key_.has_value() != username_password_.has_value());
  if (api_key_) {
    client.setRequestHeader("Authorization", "ApiKey " + *api_key_);
  } else if (username_password_) {
    client.setBasicAuth(username_password_->first, username_password_->second);
  }
}

REGISTER_RESOURCE(ElasticsearchCredentialsControllerService, ControllerService);
}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
