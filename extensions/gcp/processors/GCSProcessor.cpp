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

#include "GCSProcessor.h"

#include "../controllerservices/GCPCredentialsControllerService.h"
#include "api/utils/ProcessorConfigUtils.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

std::shared_ptr<google::cloud::Credentials> GCSProcessor::getCredentials(const api::core::ProcessContext& context) {
  if (const auto gcp_credentials_controller_service = api::utils::parseOptionalControllerService<GCPCredentialsControllerService>(context,
          GCPCredentials)) {
    return gcp_credentials_controller_service->getCredentials();
  }
  return nullptr;
}

MinifiStatus GCSProcessor::onScheduleImpl(api::core::ProcessContext& context) {
  if (const auto number_of_retries = api::utils::parseOptionalU64Property(context, NumberOfRetries)) {
    retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(gsl::narrow<int>(*number_of_retries));
  }

  gcp_credentials_ = getCredentials(context);
  if (!gcp_credentials_) {
    logger_->log_error("Couldnt find valid credentials");
    return MINIFI_STATUS_UNKNOWN_ERROR;
  }

  endpoint_url_ = context.getProperty(EndpointOverrideURL, nullptr) | utils::toOptional();
  if (endpoint_url_)
    logger_->log_debug("Endpoint overwritten: {}", *endpoint_url_);

  const auto proxy_data = context.getProxyData(ProxyConfigurationService) | utils::orThrow("Couldnt query ProxyConfigurationService");
  if (proxy_data && proxy_data->proxy_type != api::utils::ProxyType::DIRECT) {
    logger_->log_debug("Proxy configuration is set for GCS processor");

    proxy_ = google::cloud::ProxyConfig{};
    proxy_->set_scheme(minifi::utils::string::startsWith(proxy_data->host, "https") ? "https" : "http");
    auto proxy_host = proxy_data->host;
    constexpr std::string_view https_prefix = "https://";
    constexpr std::string_view http_prefix = "http://";
    if (minifi::utils::string::startsWith(proxy_host, https_prefix)) {
      proxy_host = proxy_host.substr(https_prefix.size());
    } else if (minifi::utils::string::startsWith(proxy_host, http_prefix)) {
      proxy_host = proxy_host.substr(http_prefix.size());
    }
    proxy_->set_hostname(proxy_host);
    proxy_->set_port(std::to_string(proxy_data->port));
    if (auto proxy_credentials = proxy_data->proxy_credentials) {
      proxy_->set_username(proxy_credentials->username);
      proxy_->set_password(proxy_credentials->password);
    }
  }
  return MINIFI_STATUS_SUCCESS;
}

gcs::Client GCSProcessor::getClient() const {
  auto options = google::cloud::Options{}
      .set<google::cloud::UnifiedCredentialsOption>(gcp_credentials_)
      .set<google::cloud::storage::RetryPolicyOption>(retry_policy_);

  if (proxy_) {
    options.set<google::cloud::ProxyOption>(*proxy_);
  }

  if (endpoint_url_) {
    options.set<gcs::RestEndpointOption>(*endpoint_url_);
  }
  return gcs::Client(options);
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
