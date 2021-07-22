/**
 * @file PutS3Object.cpp
 * PutS3Object class implementation
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

#include "PutS3Object.h"

#include <string>
#include <set>
#include <memory>
#include <utility>

#include "AWSCredentialsService.h"
#include "properties/Properties.h"
#include "utils/StringUtils.h"
#include "utils/MapUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const uint64_t PutS3Object::ReadCallback::MAX_SIZE = 5UL * 1024UL * 1024UL * 1024UL;  // 5GB limit on AWS
const uint64_t PutS3Object::ReadCallback::BUFFER_SIZE = 4096;

const std::set<std::string> PutS3Object::CANNED_ACLS(minifi::utils::MapUtils::getKeys(minifi::aws::s3::CANNED_ACL_MAP));
const std::set<std::string> PutS3Object::STORAGE_CLASSES(minifi::utils::MapUtils::getKeys(minifi::aws::s3::STORAGE_CLASS_MAP));
const std::set<std::string> PutS3Object::SERVER_SIDE_ENCRYPTIONS(minifi::utils::MapUtils::getKeys(minifi::aws::s3::SERVER_SIDE_ENCRYPTION_MAP));

const core::Property PutS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ContentType(
  core::PropertyBuilder::createProperty("Content Type")
    ->withDescription("Sets the Content-Type HTTP header indicating the type of content stored in "
                      "the associated object. The value of this header is a standard MIME type. "
                      "If no content type is provided the default content type "
                      "\"application/octet-stream\" will be used.")
    ->supportsExpressionLanguage(true)
    ->withDefaultValue<std::string>("application/octet-stream")
    ->build());
const core::Property PutS3Object::StorageClass(
  core::PropertyBuilder::createProperty("Storage Class")
    ->isRequired(true)
    ->withDefaultValue<std::string>("Standard")
    ->withAllowableValues<std::string>(PutS3Object::STORAGE_CLASSES)
    ->withDescription("AWS S3 Storage Class")
    ->build());
const core::Property PutS3Object::FullControlUserList(
  core::PropertyBuilder::createProperty("FullControl User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Full Control for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ReadPermissionUserList(
  core::PropertyBuilder::createProperty("Read Permission User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have Read Access for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ReadACLUserList(
  core::PropertyBuilder::createProperty("Read ACL User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to read "
                      "the Access Control List for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::WriteACLUserList(
  core::PropertyBuilder::createProperty("Write ACL User List")
    ->withDescription("A comma-separated list of Amazon User ID's or E-mail addresses that specifies who should have permissions to change "
                      "the Access Control List for an object.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::CannedACL(
  core::PropertyBuilder::createProperty("Canned ACL")
    ->withDescription("Amazon Canned ACL for an object. Allowed values: BucketOwnerFullControl, BucketOwnerRead, AuthenticatedRead, "
                      "PublicReadWrite, PublicRead, Private, AwsExecRead; will be ignored if any other ACL/permission property is specified.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property PutS3Object::ServerSideEncryption(
  core::PropertyBuilder::createProperty("Server Side Encryption")
    ->isRequired(true)
    ->withDefaultValue<std::string>("None")
    ->withAllowableValues<std::string>(PutS3Object::SERVER_SIDE_ENCRYPTIONS)
    ->withDescription("Specifies the algorithm used for server side encryption.")
    ->build());

const core::Relationship PutS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship PutS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

void PutS3Object::initialize() {
  // Add new supported properties
  updateSupportedProperties({ObjectKey, ContentType, StorageClass, FullControlUserList, ReadPermissionUserList,
    ReadACLUserList, WriteACLUserList, CannedACL, ServerSideEncryption});
  // Set the supported relationships
  setSupportedRelationships({Failure, Success});
}

void PutS3Object::fillUserMetadata(const std::shared_ptr<core::ProcessContext> &context) {
  const auto &dynamic_prop_keys = context->getDynamicPropertyKeys();
  bool first_property = true;
  for (const auto &prop_key : dynamic_prop_keys) {
    std::string prop_value = "";
    if (context->getDynamicProperty(prop_key, prop_value) && !prop_value.empty()) {
      logger_->log_debug("PutS3Object: DynamicProperty: [%s] -> [%s]", prop_key, prop_value);
      user_metadata_map_.emplace(prop_key, prop_value);
      if (first_property) {
        user_metadata_ = prop_key + "=" + prop_value;
        first_property = false;
      } else {
        user_metadata_ += "," + prop_key + "=" + prop_value;
      }
    }
  }
  logger_->log_debug("PutS3Object: User metadata [%s]", user_metadata_);
}

void PutS3Object::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);

  if (!context->getProperty(StorageClass.getName(), storage_class_)
      || storage_class_.empty()
      || STORAGE_CLASSES.find(storage_class_) == STORAGE_CLASSES.end()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Storage Class property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Storage Class [%s]", storage_class_);

  if (!context->getProperty(ServerSideEncryption.getName(), server_side_encryption_)
      || server_side_encryption_.empty()
      || SERVER_SIDE_ENCRYPTIONS.find(server_side_encryption_) == SERVER_SIDE_ENCRYPTIONS.end()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Server Side Encryption property missing or invalid");
  }
  logger_->log_debug("PutS3Object: Server Side Encryption [%s]", server_side_encryption_);

  fillUserMetadata(context);
}

std::string PutS3Object::parseAccessControlList(const std::string &comma_separated_list) const {
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
  if (!put_s3_request_params.canned_acl.empty() && CANNED_ACLS.find(put_s3_request_params.canned_acl) == CANNED_ACLS.end()) {
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
  aws::s3::PutObjectRequestParameters params(common_properties.credentials, client_config_);
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
  logger_->log_debug("PutS3Object onTrigger");
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
  session->read(flow_file, &callback);
  if (!callback.result_.has_value()) {
    logger_->log_error("Failed to upload S3 object to bucket '%s'", put_s3_request_params->bucket);
    session->transfer(flow_file, Failure);
  } else {
    setAttributes(session, flow_file, *put_s3_request_params, *callback.result_);
    logger_->log_debug("Successfully uploaded S3 object '%s' to bucket '%s'", put_s3_request_params->object_key, put_s3_request_params->bucket);
    session->transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(PutS3Object, "This Processor puts FlowFiles to an Amazon S3 Bucket.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
