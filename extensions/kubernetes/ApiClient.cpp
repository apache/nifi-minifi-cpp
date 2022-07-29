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
#include "ApiClient.h"

extern "C" {
#include "config/incluster_config.h"
#include "config/kube_config.h"
#include "include/apiClient.h"
}

#include "utils/StringUtils.h"

namespace {

gsl::not_null<apiClient_t*> createApiClient(char **base_path, sslConfig_t** ssl_config, list_t** api_keys) {
  int rc = load_incluster_config(base_path, ssl_config, api_keys);
  if (rc != 0) {
    throw std::runtime_error(org::apache::nifi::minifi::utils::StringUtils::join_pack("load_incluster_config() failed with error code ", std::to_string(rc)));
  }
  const auto api_client = apiClient_create_with_base_path(*base_path, *ssl_config, *api_keys);
  if (!api_client) {
    throw std::runtime_error("apiClient_create_with_base_path() failed");
  }
  return gsl::make_not_null(api_client);
}

}  // namespace

namespace org::apache::nifi::minifi::kubernetes {

ApiClient::ApiClient() : api_client_(createApiClient(&base_path_, &ssl_config_, &api_keys_)) {}

ApiClient::~ApiClient() noexcept {
  apiClient_free(api_client_);
  free_client_config(base_path_, ssl_config_, api_keys_);
  apiClient_unsetupGlobalEnv();
}

}  // namespace org::apache::nifi::minifi::kubernetes
