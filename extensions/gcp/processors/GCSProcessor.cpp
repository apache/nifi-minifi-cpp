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

  endpoint_url_ = context.getProperty(EndpointOverrideURL) | utils::toOptional();
  if (endpoint_url_) { logger_->log_debug("Endpoint overwritten: {}", *endpoint_url_); }
  return MINIFI_STATUS_SUCCESS;
}

gcs::Client GCSProcessor::getClient() const {
  auto options = google::cloud::Options{}
      .set<google::cloud::UnifiedCredentialsOption>(gcp_credentials_)
      .set<google::cloud::storage::RetryPolicyOption>(retry_policy_);

  if (endpoint_url_) { options.set<gcs::RestEndpointOption>(*endpoint_url_); }
  return gcs::Client(options);
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
