/**
 * @file FetchS3Object.cpp
 * FetchS3Object class implementation
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
#include "core/Resource.h"

#include <set>
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const core::Property FetchS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property FetchS3Object::Version(
  core::PropertyBuilder::createProperty("Version")
    ->withDescription("The Version of the Object to download")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property FetchS3Object::RequesterPays(
  core::PropertyBuilder::createProperty("Requester Pays")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If true, indicates that the requester consents to pay any charges associated with retrieving "
                      "objects from the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'.")
    ->build());

const core::Relationship FetchS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship FetchS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

void FetchS3Object::initialize() {
  // Add new supported properties
  updateSupportedProperties({ObjectKey, Version, RequesterPays});
  // Set the supported relationships
  setSupportedRelationships({Failure, Success});
}

void FetchS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);

  context->getProperty(RequesterPays.getName(), requester_pays_);
  logger_->log_debug("FetchS3Object: RequesterPays [%s]", requester_pays_ ? "true" : "false");
}

std::optional<aws::s3::GetObjectRequestParameters> FetchS3Object::buildFetchS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const {
  minifi::aws::s3::GetObjectRequestParameters get_object_params(common_properties.credentials, client_config_);
  get_object_params.bucket = common_properties.bucket;
  get_object_params.requester_pays = requester_pays_;

  context->getProperty(ObjectKey, get_object_params.object_key, flow_file);
  if (get_object_params.object_key.empty() && (!flow_file->getAttribute("filename", get_object_params.object_key) || get_object_params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("FetchS3Object: Object Key [%s]", get_object_params.object_key);

  context->getProperty(Version, get_object_params.version, flow_file);
  logger_->log_debug("FetchS3Object: Version [%s]", get_object_params.version);
  get_object_params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  return get_object_params;
}

void FetchS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_debug("FetchS3Object onTrigger");
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

  auto get_object_params = buildFetchS3RequestParams(context, flow_file, *common_properties);
  if (!get_object_params) {
    session->transfer(flow_file, Failure);
    return;
  }

  WriteCallback callback(flow_file->getSize(), *get_object_params, s3_wrapper_);
  session->write(flow_file, &callback);

  if (callback.result_) {
    auto putAttributeIfNotEmpty = [&](const std::string& attribute, const std::string& value) {
      if (!value.empty()) {
        session->putAttribute(flow_file, attribute, value);
      }
    };

    logger_->log_debug("Successfully fetched S3 object %s from bucket %s", get_object_params->object_key, get_object_params->bucket);
    session->putAttribute(flow_file, "s3.bucket", get_object_params->bucket);
    session->putAttribute(flow_file, core::SpecialFlowAttribute::PATH, callback.result_->path);
    session->putAttribute(flow_file, core::SpecialFlowAttribute::ABSOLUTE_PATH, callback.result_->absolute_path);
    session->putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, callback.result_->filename);
    putAttributeIfNotEmpty(core::SpecialFlowAttribute::MIME_TYPE, callback.result_->mime_type);
    putAttributeIfNotEmpty("s3.etag", callback.result_->etag);
    putAttributeIfNotEmpty("s3.expirationTime", callback.result_->expiration.expiration_time);
    putAttributeIfNotEmpty("s3.expirationTimeRuleId", callback.result_->expiration.expiration_time_rule_id);
    putAttributeIfNotEmpty("s3.sseAlgorithm", callback.result_->ssealgorithm);
    putAttributeIfNotEmpty("s3.version", callback.result_->version);
    session->transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to fetch S3 object %s from bucket %s", get_object_params->object_key, get_object_params->bucket);
    session->transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(FetchS3Object, "This Processor retrieves the contents of an S3 Object and writes it to the content of a FlowFile.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
