/**
 *
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

#include "FetchS3Object.h"

#include <memory>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/OptionalUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

void FetchS3Object::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void FetchS3Object::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  S3Processor::onSchedule(context, session_factory);

  requester_pays_ = minifi::utils::parseBoolProperty(context, RequesterPays);
  logger_->log_debug("FetchS3Object: RequesterPays [{}]", requester_pays_);
}

std::optional<aws::s3::GetObjectRequestParameters> FetchS3Object::buildFetchS3RequestParams(
    const core::ProcessContext& context,
    const core::FlowFile& flow_file,
    const CommonProperties &common_properties,
    const std::string_view bucket) const {
  gsl_Expects(client_config_);
  minifi::aws::s3::GetObjectRequestParameters get_object_params(common_properties.credentials, *client_config_);
  get_object_params.bucket = bucket;
  get_object_params.requester_pays = requester_pays_;

  if (const auto object_key = context.getProperty(ObjectKey, &flow_file)) {
    get_object_params.object_key = * object_key;
  }
  if (get_object_params.object_key.empty() && (!flow_file.getAttribute("filename", get_object_params.object_key) || get_object_params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("FetchS3Object: Object Key [{}]", get_object_params.object_key);

  if (const auto version = context.getProperty(Version, &flow_file)) {
    get_object_params.version = *version;
  }
  logger_->log_debug("FetchS3Object: Version [{}]", get_object_params.version);
  get_object_params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  return get_object_params;
}

void FetchS3Object::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("FetchS3Object onTrigger");
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

  auto get_object_params = buildFetchS3RequestParams(context, *flow_file, *common_properties, *bucket);
  if (!get_object_params) {
    session.transfer(flow_file, Failure);
    return;
  }

  std::optional<minifi::aws::s3::GetObjectResult> result;
  session.write(flow_file, [&get_object_params, &result, this](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
    result = s3_wrapper_.getObject(*get_object_params, *stream);
    return (result | minifi::utils::transform(&s3::GetObjectResult::write_size)).value_or(0);
  });

  if (result) {
    auto putAttributeIfNotEmpty = [&](std::string_view attribute, const std::string& value) {
      if (!value.empty()) {
        session.putAttribute(*flow_file, attribute, value);
      }
    };

    logger_->log_debug("Successfully fetched S3 object {} from bucket {}", get_object_params->object_key, get_object_params->bucket);
    session.putAttribute(*flow_file, "s3.bucket", get_object_params->bucket);
    session.putAttribute(*flow_file, core::SpecialFlowAttribute::PATH, result->path.generic_string());
    session.putAttribute(*flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, result->absolute_path.generic_string());
    session.putAttribute(*flow_file, core::SpecialFlowAttribute::FILENAME, result->filename.generic_string());
    putAttributeIfNotEmpty(core::SpecialFlowAttribute::MIME_TYPE, result->mime_type);
    putAttributeIfNotEmpty("s3.etag", result->etag);
    putAttributeIfNotEmpty("s3.expirationTime", result->expiration.expiration_time);
    putAttributeIfNotEmpty("s3.expirationTimeRuleId", result->expiration.expiration_time_rule_id);
    putAttributeIfNotEmpty("s3.sseAlgorithm", result->ssealgorithm);
    putAttributeIfNotEmpty("s3.version", result->version);
    session.transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to fetch S3 object {} from bucket {}", get_object_params->object_key, get_object_params->bucket);
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(FetchS3Object, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
