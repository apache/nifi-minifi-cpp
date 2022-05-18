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

#include "core/Resource.h"
#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "utils/OptionalUtils.h"
#include "../GCPAttributes.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
const core::Property ListGCSBucket::Bucket(
    core::PropertyBuilder::createProperty("Bucket")
        ->withDescription("Bucket of the object.")
        ->isRequired(true)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property ListGCSBucket::ListAllVersions(
    core::PropertyBuilder::createProperty("List all versions")
        ->withDescription("Set this option to `true` to get all the previous versions separately.")
        ->withDefaultValue<bool>(false)
        ->build());

const core::Relationship ListGCSBucket::Success("success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation.");

void ListGCSBucket::initialize() {
  setSupportedProperties({GCPCredentials,
                          Bucket,
                          NumberOfRetries,
                          EndpointOverrideURL,
                          ListAllVersions});
  setSupportedRelationships({Success});
}


void ListGCSBucket::onSchedule(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSessionFactory>& session_factory) {
  GCSProcessor::onSchedule(context, session_factory);
  gsl_Expects(context);
  context->getProperty(Bucket.getName(), bucket_);
}

void ListGCSBucket::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && gcp_credentials_);

  gcs::Client client = getClient();
  auto list_all_versions = context->getProperty<bool>(ListAllVersions);
  gcs::Versions versions = (list_all_versions && *list_all_versions) ? gcs::Versions(true) : gcs::Versions(false);
  auto objects_in_bucket = client.ListObjects(bucket_, versions);
  for (const auto& object_in_bucket : objects_in_bucket) {
    if (object_in_bucket.ok()) {
      auto flow_file = session->create();
      flow_file->updateAttribute(core::SpecialFlowAttribute::FILENAME, object_in_bucket->name());
      setAttributesFromObjectMetadata(*flow_file, *object_in_bucket);
      session->transfer(flow_file, Success);
    } else {
      logger_->log_error("Invalid object in bucket %s", bucket_);
    }
  }
}

REGISTER_RESOURCE(ListGCSBucket, "Retrieves a listing of objects from an GCS bucket. "
                                 "For each object that is listed, creates a FlowFile that represents the object so that it can be fetched in conjunction with FetchGCSObject.");
}  // namespace org::apache::nifi::minifi::extensions::gcp
