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

#include "utils/ProcessorConfigUtils.h"

#include "../GCPAttributes.h"
#include "minifi-cpp/core/FlowFile.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
void ListGCSBucket::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}


void ListGCSBucket::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  GCSProcessor::onSchedule(context, session_factory);
  bucket_ = utils::parseProperty(context, Bucket);
}

void ListGCSBucket::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(gcp_credentials_);

  gcs::Client client = getClient();
  auto list_all_versions = utils::parseOptionalBoolProperty(context, ListAllVersions);
  gcs::Versions versions = (list_all_versions && *list_all_versions) ? gcs::Versions(true) : gcs::Versions(false);
  auto objects_in_bucket = client.ListObjects(bucket_, versions);
  for (const auto& object_in_bucket : objects_in_bucket) {
    if (object_in_bucket.ok()) {
      auto flow_file = session.create();
      flow_file->updateAttribute(core::SpecialFlowAttribute::FILENAME, object_in_bucket->name());
      setAttributesFromObjectMetadata(*flow_file, *object_in_bucket);
      session.transfer(flow_file, Success);
    } else {
      logger_->log_error("Invalid object in bucket {}", bucket_);
    }
  }
}

REGISTER_RESOURCE(ListGCSBucket, Processor);

}  // namespace org::apache::nifi::minifi::extensions::gcp
