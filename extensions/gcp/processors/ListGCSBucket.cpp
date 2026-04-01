/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ListGCSBucket.h"

#include "../GCPAttributes.h"
#include "api/core/ProcessContext.h"
#include "api/core/ProcessSession.h"
#include "api/core/Resource.h"
#include "api/utils/ProcessorConfigUtils.h"
#include "minifi-cpp/core/SpecialFlowAttribute.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

MinifiStatus ListGCSBucket::onScheduleImpl(api::core::ProcessContext& context) {
  const auto status = GCSProcessor::onScheduleImpl(context);
  if (status != MinifiStatus::MINIFI_STATUS_SUCCESS) {
    return status;
  }
  bucket_ = api::utils::parseProperty(context, Bucket);
  return MinifiStatus::MINIFI_STATUS_SUCCESS;
}

MinifiStatus ListGCSBucket::onTriggerImpl(api::core::ProcessContext& context, api::core::ProcessSession& session) {
  gsl_Expects(gcp_credentials_);

  gcs::Client client = getClient();
  auto list_all_versions = api::utils::parseOptionalBoolProperty(context, ListAllVersions);
  gcs::Versions versions = (list_all_versions && *list_all_versions) ? gcs::Versions(true) : gcs::Versions(false);
  auto objects_in_bucket = client.ListObjects(bucket_, versions);
  for (const auto& object_in_bucket : objects_in_bucket) {
    if (object_in_bucket.ok()) {
      auto flow_file = session.create();
      session.setAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, object_in_bucket->name());
      setAttributesFromObjectMetadata(flow_file, *object_in_bucket, session);
      session.transfer(std::move(flow_file), Success);
    } else {
      logger_->log_error("Invalid object in bucket {}", bucket_);
    }
  }
  return MinifiStatus::MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
