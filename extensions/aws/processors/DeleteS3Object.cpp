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

#include "DeleteS3Object.h"

#include <set>
#include <memory>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::aws::processors {

void DeleteS3Object::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

std::optional<aws::s3::DeleteObjectRequestParameters> DeleteS3Object::buildDeleteS3RequestParams(
    const core::ProcessContext& context,
    const core::FlowFile& flow_file,
    const CommonProperties& common_properties,
    const std::string_view bucket) const {
  gsl_Expects(client_config_);
  aws::s3::DeleteObjectRequestParameters params(common_properties.credentials, *client_config_);
  if (const auto object_key = context.getProperty(ObjectKey, &flow_file)) {
    params.object_key = *object_key;
  }
  if (params.object_key.empty() && (!flow_file.getAttribute("filename", params.object_key) || params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("DeleteS3Object: Object Key [{}]", params.object_key);

  if (const auto version = context.getProperty(Version, &flow_file)) {
    params.version = *version;
  }
  logger_->log_debug("DeleteS3Object: Version [{}]", params.version);

  params.bucket = bucket;
  params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  return params;
}

void DeleteS3Object::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("DeleteS3Object onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto common_properties = getCommonELSupportedProperties(context, flow_file.get());
  if (!common_properties) {
    session.transfer(flow_file, Failure);
    return;
  }

  auto bucket = context.getProperty(Bucket.name, flow_file.get());
  if (!bucket) {
    logger_->log_error("Bucket is invalid due to {}", bucket.error().message());
    session.transfer(flow_file, Failure);
    return;
  }
  logger_->log_debug("S3Processor: Bucket [{}]", *bucket);

  auto params = buildDeleteS3RequestParams(context, *flow_file, *common_properties, *bucket);
  if (!params) {
    session.transfer(flow_file, Failure);
    return;
  }

  if (s3_wrapper_.deleteObject(*params)) {
    logger_->log_debug("Successfully deleted S3 object '{}' from bucket '{}'", params->object_key, *bucket);
    session.transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to delete S3 object '{}' from bucket '{}'", params->object_key, *bucket);
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(DeleteS3Object, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
