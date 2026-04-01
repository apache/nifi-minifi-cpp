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

#include "DeleteGCSObject.h"


#include "../GCPAttributes.h"
#include "api/core/ProcessContext.h"
#include "api/core/ProcessSession.h"
#include "api/core/Resource.h"
#include "api/utils/ProcessorConfigUtils.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

MinifiStatus DeleteGCSObject::onTriggerImpl(api::core::ProcessContext& context, api::core::ProcessSession& session) {
  gsl_Expects(gcp_credentials_);

  auto flow_file = session.get();
  if (!flow_file) {
    return MINIFI_STATUS_PROCESSOR_YIELD;
  }

  auto bucket = api::utils::parseOptionalProperty(context, Bucket, &flow_file);
  if (!bucket || bucket->empty()) {
    logger_->log_error("Missing bucket name");
    session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
  }
  auto object_name = api::utils::parseOptionalProperty(context, Key, &flow_file);
  if (!object_name || object_name->empty()) {
    logger_->log_error("Missing object name");
    session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
  }

  gcs::Generation generation;
  if (auto object_generation_str = api::utils::parseOptionalProperty(context, ObjectGeneration, &flow_file); object_generation_str && !object_generation_str->empty()) {
    if (const auto geni64 = parsing::parseIntegral<int64_t>(*object_generation_str)) {
      generation = gcs::Generation{*geni64};
    } else {
      logger_->log_error("Invalid generation: {}", *object_generation_str);
      session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
    }
  }

  auto status = getClient().DeleteObject(*bucket, *object_name, generation, gcs::IfGenerationNotMatch(0));

  if (!status.ok()) {
    session.setAttribute(flow_file, GCS_STATUS_MESSAGE, status.message());
    session.setAttribute(flow_file, GCS_ERROR_REASON, status.error_info().reason());
    session.setAttribute(flow_file, GCS_ERROR_DOMAIN, status.error_info().domain());
    logger_->log_error("Failed to delete {} object from {} bucket on Google Cloud Storage {} {}", *object_name, *bucket, status.message(), status.error_info().reason());
    session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
  }

  session.transfer(std::move(flow_file), Success);
  return MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
