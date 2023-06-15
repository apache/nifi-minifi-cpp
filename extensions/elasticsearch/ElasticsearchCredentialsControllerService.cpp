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

#include <utility>

#include "ElasticsearchCredentialsControllerService.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

void ElasticsearchCredentialsControllerService::initialize() {
  setSupportedProperties(Properties);
}

void ElasticsearchCredentialsControllerService::onEnable() {
  getProperty(ApiKey, api_key_);
  std::string username;
  std::string password;
  getProperty(Username, username);
  getProperty(Password, password);
  if (!username.empty() && !password.empty())
    username_password_.emplace(std::move(username), std::move(password));
  if (api_key_.has_value() == username_password_.has_value())
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Either an API Key or Username and Password must be provided");
}

void ElasticsearchCredentialsControllerService::authenticateClient(curl::HTTPClient& client) {
  gsl_Expects(api_key_.has_value() != username_password_.has_value());
  if (api_key_) {
    client.setRequestHeader("Authorization", "ApiKey " + *api_key_);
  } else if (username_password_) {
    client.setBasicAuth(username_password_->first, username_password_->second);
  }
}

REGISTER_RESOURCE(ElasticsearchCredentialsControllerService, ControllerService);
}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
