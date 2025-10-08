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

#include "utils/ProcessorConfigUtils.h"

#include "../controllerservices/GCPCredentialsControllerService.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

std::shared_ptr<google::cloud::Credentials> GCSProcessor::getCredentials(core::ProcessContext& context) const {
  auto gcp_credentials_controller_service = utils::parseOptionalControllerService<GCPCredentialsControllerService>(context, GCSProcessor::GCPCredentials, getUUID());
  if (gcp_credentials_controller_service) {
    return gcp_credentials_controller_service->getCredentials();
  }
  return nullptr;
}

void GCSProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  if (auto number_of_retries = utils::parseOptionalU64Property(context, NumberOfRetries)) {
    retry_policy_ = std::make_shared<google::cloud::storage::LimitedErrorCountRetryPolicy>(gsl::narrow<int>(*number_of_retries));
  }

  gcp_credentials_ = getCredentials(context);
  if (!gcp_credentials_) {
    throw minifi::Exception(ExceptionType::PROCESS_SCHEDULE_EXCEPTION, "Missing GCP Credentials");
  }

  endpoint_url_ = context.getProperty(EndpointOverrideURL) | utils::toOptional();
  if (endpoint_url_)
    logger_->log_debug("Endpoint overwritten: {}", *endpoint_url_);
}

gcs::Client GCSProcessor::getClient() const {
  auto options = google::cloud::Options{}
      .set<google::cloud::UnifiedCredentialsOption>(gcp_credentials_)
      .set<google::cloud::storage::RetryPolicyOption>(retry_policy_);

  if (endpoint_url_) {
    options.set<gcs::RestEndpointOption>(*endpoint_url_);
  }
  return gcs::Client(options);
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
