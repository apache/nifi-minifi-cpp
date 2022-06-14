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
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

std::optional<aws::s3::DeleteObjectRequestParameters> DeleteS3Object::buildDeleteS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const {
  gsl_Expects(client_config_);
  aws::s3::DeleteObjectRequestParameters params(common_properties.credentials, *client_config_);
  context->getProperty(ObjectKey, params.object_key, flow_file);
  if (params.object_key.empty() && (!flow_file->getAttribute("filename", params.object_key) || params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("DeleteS3Object: Object Key [%s]", params.object_key);

  context->getProperty(Version, params.version, flow_file);
  logger_->log_debug("DeleteS3Object: Version [%s]", params.version);

  params.bucket = common_properties.bucket;
  params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  return params;
}

void DeleteS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("DeleteS3Object onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto common_properties = getCommonELSupportedProperties(context, flow_file);
  if (!common_properties) {
    session->transfer(flow_file, Failure);
    return;
  }

  auto params = buildDeleteS3RequestParams(context, flow_file, *common_properties);
  if (!params) {
    session->transfer(flow_file, Failure);
    return;
  }

  if (s3_wrapper_.deleteObject(*params)) {
    logger_->log_debug("Successfully deleted S3 object '%s' from bucket '%s'", params->object_key, common_properties->bucket);
    session->transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to delete S3 object '%s' from bucket '%s'", params->object_key, common_properties->bucket);
    session->transfer(flow_file, Failure);
  }
}

}  // namespace org::apache::nifi::minifi::aws::processors
