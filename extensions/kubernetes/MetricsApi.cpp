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
#include "MetricsApi.h"

#include <memory>

extern "C" {
#include "include/apiClient.h"
#include "include/list.h"
}

#include "ApiClient.h"
#include "utils/StringUtils.h"

namespace {
struct list_t_deleter {
  void operator()(list_t* ptr) const noexcept { list_freeList(ptr); }
};
using list_unique_ptr = std::unique_ptr<list_t, list_t_deleter>;
}  // namespace

namespace org::apache::nifi::minifi::kubernetes::metrics {

nonstd::expected<std::string, std::string> podMetricsList(const kubernetes::ApiClient& api_client) {
  list_unique_ptr localVarQueryParameters;
  list_unique_ptr localVarHeaderParameters;
  list_unique_ptr localVarFormParameters;
  list_unique_ptr localVarHeaderType{list_createList()};
  list_unique_ptr localVarContentType;
  char* localVarBodyParameters = nullptr;

  std::string url_path = "/apis/metrics.k8s.io/v1beta1/pods";
  char* localVarPath = const_cast<char*>(url_path.c_str());

  std::string header_type = "application/json";
  list_addElement(localVarHeaderType.get(), const_cast<char*>(header_type.c_str()));

  std::string verb = "GET";
  char* request_type = const_cast<char*>(verb.c_str());

  apiClient_invoke(api_client.getClient(),
                  localVarPath,
                  localVarQueryParameters.get(),
                  localVarHeaderParameters.get(),
                  localVarFormParameters.get(),
                  localVarHeaderType.get(),
                  localVarContentType.get(),
                  localVarBodyParameters,
                  request_type);

  std::string result;
  if (api_client.getClient()->dataReceived) {
    result = std::string{static_cast<const char*>(api_client.getClient()->dataReceived)};
    free(api_client.getClient()->dataReceived);
    api_client.getClient()->dataReceived = nullptr;
    api_client.getClient()->dataReceivedLen = 0;
  }

  const auto is_success = [](auto response_code) { return response_code >= 200 && response_code < 300; };
  if (!is_success(api_client.getClient()->response_code)) {
    const auto error_code = std::to_string(api_client.getClient()->response_code);
    return nonstd::make_unexpected(utils::StringUtils::join_pack("Error from Kubernetes API ", url_path, ": error code ", error_code, ", data received: ", result));
  }

  return result;
}

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
