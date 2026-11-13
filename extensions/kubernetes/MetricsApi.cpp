// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "MetricsApi.h"

#include <memory>

extern "C" {
#include "kubernetes/include/generic.h"
}

#include "ApiClient.h"
#include "utils/Deleters.h"

namespace {
struct genericClient_t_deleter { void operator()(genericClient_t* ptr) const noexcept { genericClient_free(ptr); } };
using genericClient_unique_ptr = std::unique_ptr<genericClient_t, genericClient_t_deleter>;
}  // namespace

namespace org::apache::nifi::minifi::kubernetes::metrics {

std::expected<std::string, std::string> podMetricsList(const kubernetes::ApiClient& api_client) {
  genericClient_unique_ptr genericClient{genericClient_create(api_client.getClient(), "metrics.k8s.io", "v1beta1", "pods")};
  utils::freeing_unique_ptr<char> api_response{Generic_list(genericClient.get())};
  if (api_response) {
    return std::string{api_response.get()};
  } else {
    return std::unexpected{"Could not access the Kubernetes API /apis/metrics.k8s.io/v1beta1/pods"};
  }
}

}  // namespace org::apache::nifi::minifi::kubernetes::metrics
