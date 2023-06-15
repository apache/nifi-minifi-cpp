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

#include "PutS3Object.h"

#include <string>
#include <memory>

#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"
#include "utils/MapUtils.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "range/v3/algorithm/contains.hpp"

namespace org::apache::nifi::minifi::aws::processors {

void PutS3Object::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutS3Object::fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context) {
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  bool first_property = true;
  for (const auto &prop_key : dynamic_prop_keys) {
    std::string prop_value;
    if (context->getDynamicProperty(prop_key, prop_value) && !prop_value.empty()) {
      logger_->log_debug("PutS3Object: DynamicProperty: [%s] -> [%s]", prop_key, prop_value);
      user_metadata_map_.emplace(prop_key, prop_value);
      if (first_property) {
        user_metadata_ = minifi::utils::StringUtils::join_pack(prop_key, "=", prop_value);
        first_property = false;
      } else {
        user_metadata_ += minifi::utils::StringUtils::join_pack(",", prop_key, "=", prop_value);
      }
    }
  }
  logger_->log_debug("PutS3Object: User metadata [%s]", user_metadata_);
}

void PutS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);

  if (!context->getProperty(StorageClass, storage_class_)
      || storage_class_.empty()
      || !ranges::contains(STORAGE_CLASSES, storage_class_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Class property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Storage Class [%s]", storage_class_);

  if (!context->getProperty(ServerSideEncryption, server_side_encryption_)
      || server_side_encryption_.empty()
      || !ranges::contains(SERVER_SIDE_ENCRYPTIONS, server_side_encryption_)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Server Side Encryption property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Server Side Encryption [%s]", server_side_encryption_);

  if (auto use_path_style_access = context->getProperty<bool>(UsePathStyleAccess)) {
    use_virtual_addressing_ = !*use_path_style_access;
  }

  fillUserMetadata(context);
}

std::string PutS3Object::parseAccessControlList(const std::string &comma_separated_list) {
  auto users = minifi::utils::StringUtils::split(comma_separated_list, ",");
  for (auto& user : users) {
    auto trimmed_user = minifi::utils::StringUtils::trim(user);
    if (trimmed_user.find('@') != std::string::npos) {
      user = "emailAddress=\"" + trimmed_user + "\"";
    } else {
      user = "id=" + trimmed_user;
    }
  }
  return minifi::utils::StringUtils::join(", ", users);
}

bool PutS3Object::setCannedAcl(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    aws::s3::PutObjectRequestParameters &put_s3_request_params) const {
  context->getProperty(CannedACL, put_s3_request_params.canned_acl, flow_file);
  if (!put_s3_request_params.canned_acl.empty() && !ranges::contains(CANNED_ACLS, put_s3_request_params.canned_acl)) {
    logger_->log_error("Canned ACL is invalid!");
    return false;
  }
  logger_->log_debug("PutS3Object: Canned ACL [%s]", put_s3_request_params.canned_acl);
  return true;
}

bool PutS3Object::setAccessControl(
      const std::shared_ptr<core::ProcessContext> &context,
      const std::shared_ptr<core::FlowFile> &flow_file,
      aws::s3::PutObjectRequestParameters &put_s3_request_params) const {
  std::string value;
  if (context->getProperty(FullControlUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.fullcontrol_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Full Control User List [%s]", value);
  }
  if (context->getProperty(ReadPermissionUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.read_permission_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Read Permission User List [%s]", value);
  }
  if (context->getProperty(ReadACLUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.read_acl_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Read ACL User List [%s]", value);
  }
  if (context->getProperty(WriteACLUserList, value, flow_file) && !value.empty()) {
    put_s3_request_params.write_acl_user_list = parseAccessControlList(value);
    logger_->log_debug("PutS3Object: Write ACL User List [%s]", value);
  }

  return setCannedAcl(context, flow_file, put_s3_request_params);
}

std::optional<aws::s3::PutObjectRequestParameters> PutS3Object::buildPutS3RequestParams(
    const std::shared_ptr<core::ProcessContext> &context,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const CommonProperties &common_properties) const {
  gsl_Expects(client_config_);
  aws::s3::PutObjectRequestParameters params(common_properties.credentials, *client_config_);
  params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  params.bucket = common_properties.bucket;
  params.user_metadata_map = user_metadata_map_;
  params.server_side_encryption = server_side_encryption_;
  params.storage_class = storage_class_;

  context->getProperty(ObjectKey, params.object_key, flow_file);
  if (params.object_key.empty() && (!flow_file->getAttribute("filename", params.object_key) || params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("PutS3Object: Object Key [%s]", params.object_key);

  context->getProperty(ContentType, params.content_type, flow_file);
  logger_->log_debug("PutS3Object: Content Type [%s]", params.content_type);

  if (!setAccessControl(context, flow_file, params)) {
    return std::nullopt;
  }

  params.use_virtual_addressing = use_virtual_addressing_;
  return params;
}

void PutS3Object::setAttributes(
    const std::shared_ptr<core::ProcessSession> &session,
    const std::shared_ptr<core::FlowFile> &flow_file,
    const aws::s3::PutObjectRequestParameters &put_s3_request_params,
    const minifi::aws::s3::PutObjectResult &put_object_result) const {
  session->putAttribute(flow_file, "s3.bucket", put_s3_request_params.bucket);
  session->putAttribute(flow_file, "s3.key", put_s3_request_params.object_key);
  session->putAttribute(flow_file, "s3.contenttype", put_s3_request_params.content_type);

  if (!user_metadata_.empty()) {
    session->putAttribute(flow_file, "s3.usermetadata", user_metadata_);
  }
  if (!put_object_result.version.empty()) {
    session->putAttribute(flow_file, "s3.version", put_object_result.version);
  }
  if (!put_object_result.etag.empty()) {
    session->putAttribute(flow_file, "s3.etag", put_object_result.etag);
  }
  if (!put_object_result.expiration.empty()) {
    session->putAttribute(flow_file, "s3.expiration", put_object_result.expiration);
  }
  if (!put_object_result.ssealgorithm.empty()) {
    session->putAttribute(flow_file, "s3.sseAlgorithm", put_object_result.ssealgorithm);
  }
}

void PutS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("PutS3Object onTrigger");
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

  auto put_s3_request_params = buildPutS3RequestParams(context, flow_file, *common_properties);
  if (!put_s3_request_params) {
    session->transfer(flow_file, Failure);
    return;
  }

  PutS3Object::ReadCallback callback(flow_file->getSize(), *put_s3_request_params, s3_wrapper_);
  session->read(flow_file, std::ref(callback));
  if (!callback.result_.has_value()) {
    logger_->log_error("Failed to upload S3 object to bucket '%s'", put_s3_request_params->bucket);
    session->transfer(flow_file, Failure);
  } else {
    setAttributes(session, flow_file, *put_s3_request_params, *callback.result_);
    logger_->log_debug("Successfully uploaded S3 object '%s' to bucket '%s'", put_s3_request_params->object_key, put_s3_request_params->bucket);
    session->transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(PutS3Object, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
