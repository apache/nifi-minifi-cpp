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
#include "include/generic.h"
}

#include "ApiClient.h"
#include "utils/Deleters.h"

namespace {
struct genericClient_t_deleter { void operator()(genericClient_t* ptr) const noexcept { genericClient_free(ptr); } };
using genericClient_unique_ptr = std::unique_ptr<genericClient_t, genericClient_t_deleter>;
}  // namespace

namespace org::apache::nifi::minifi::kubernetes::metrics {

nonstd::expected<std::string, std::string> podMetricsList(const kubernetes::ApiClient& api_client) {
  genericClient_unique_ptr genericClient{genericClient_create(api_client.getClient(), "metrics.k8s.io", "v1beta1", "pods")};
  utils::freeing_unique_ptr<char> api_response{Generic_list(genericClient.get())};
  if (api_response) {
    return std::string{api_response.get()};
  } else {
    return nonstd::make_unexpected("Could not access the Kubernetes API /apis/metrics.k8s.io/v1beta1/pods");
  }
}

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
