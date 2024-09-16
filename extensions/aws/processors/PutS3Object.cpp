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
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::aws::processors {

void PutS3Object::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutS3Object::fillUserMetadata(core::ProcessContext& context) {
  const auto &dynamic_prop_keys = context.getDynamicPropertyKeys();
  bool first_property = true;
  for (const auto &prop_key : dynamic_prop_keys) {
    if (const auto prop_value = context.getDynamicProperty(prop_key); prop_value && !prop_value->empty()) {
      logger_->log_debug("PutS3Object: DynamicProperty: [{}] -> [{}]", prop_key, *prop_value);
      user_metadata_map_.emplace(prop_key, *prop_value);
      if (first_property) {
        user_metadata_ = minifi::utils::string::join_pack(prop_key, "=", *prop_value);
        first_property = false;
      } else {
        user_metadata_ += minifi::utils::string::join_pack(",", prop_key, "=", *prop_value);
      }
    }
  }
  logger_->log_debug("PutS3Object: User metadata [{}]", user_metadata_);
}

void PutS3Object::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  S3Processor::onSchedule(context, session_factory);

  storage_class_ = minifi::utils::parseProperty(context, StorageClass);
  logger_->log_debug("PutS3Object: Storage Class [{}]", storage_class_);

  server_side_encryption_ = minifi::utils::parseProperty(context, ServerSideEncryption);
  logger_->log_debug("PutS3Object: Server Side Encryption [{}]", server_side_encryption_);

  if (const auto use_path_style_access = minifi::utils::parseOptionalBoolProperty(context, UsePathStyleAccess)) {
    use_virtual_addressing_ = !*use_path_style_access;
  }

  multipart_threshold_ = context.getProperty(MultipartThreshold)
      | minifi::utils::andThen([&](const auto str) { return parsing::parseDataSizeMinMax(str, getMinPartSize(), getMaxUploadSize()); })
      | minifi::utils::expect("Multipart Part Size is not between the valid 5MB and 5GB range!");

  logger_->log_debug("PutS3Object: Multipart Threshold {}", multipart_threshold_);

  multipart_size_ = context.getProperty(MultipartPartSize)
    | minifi::utils::andThen([&](const auto str) { return parsing::parseDataSizeMinMax(str, getMinPartSize(), getMaxUploadSize()); })
    | minifi::utils::expect("Multipart Part Size is not between the valid 5MB and 5GB range!");


  logger_->log_debug("PutS3Object: Multipart Size {}", multipart_size_);


  multipart_upload_ageoff_interval_ = minifi::utils::parseMsProperty(context, MultipartUploadAgeOffInterval);
  logger_->log_debug("PutS3Object: Multipart Upload Ageoff Interval {}", multipart_upload_ageoff_interval_);

  multipart_upload_max_age_threshold_ = minifi::utils::parseMsProperty(context, MultipartUploadMaxAgeThreshold);
  logger_->log_debug("PutS3Object: Multipart Upload Max Age Threshold {}", multipart_upload_max_age_threshold_);

  fillUserMetadata(context);

  auto state_manager = context.getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  s3_wrapper_.initializeMultipartUploadStateStorage(gsl::make_not_null(state_manager));
}

std::string PutS3Object::parseAccessControlList(const std::string &comma_separated_list) {
  auto users = minifi::utils::string::split(comma_separated_list, ",");
  for (auto& user : users) {
    auto trimmed_user = minifi::utils::string::trim(user);
    if (trimmed_user.find('@') != std::string::npos) {
      user = "emailAddress=\"" + trimmed_user + "\"";
    } else {
      user = "id=" + trimmed_user;
    }
  }
  return minifi::utils::string::join(", ", users);
}

bool PutS3Object::setCannedAcl(
    core::ProcessContext& context,
    const core::FlowFile& flow_file,
    aws::s3::PutObjectRequestParameters &put_s3_request_params) const {
  if (const auto canned_acl = context.getProperty(CannedACL, &flow_file)) {
    put_s3_request_params.canned_acl = *canned_acl;
  }

  if (!put_s3_request_params.canned_acl.empty() && !ranges::contains(CANNED_ACLS, put_s3_request_params.canned_acl)) {
    logger_->log_error("Canned ACL is invalid!");
    return false;
  }
  logger_->log_debug("PutS3Object: Canned ACL [{}]", put_s3_request_params.canned_acl);
  return true;
}

bool PutS3Object::setAccessControl(
      core::ProcessContext& context,
      const core::FlowFile& flow_file,
      aws::s3::PutObjectRequestParameters &put_s3_request_params) const {
  if (const auto full_control_user_list = context.getProperty(FullControlUserList, &flow_file)) {
    put_s3_request_params.fullcontrol_user_list = parseAccessControlList(*full_control_user_list);
    logger_->log_debug("PutS3Object: Full Control User List [{}]", *full_control_user_list);
  }
  if (const auto read_permission_user_list = context.getProperty(ReadPermissionUserList, &flow_file)) {
    put_s3_request_params.read_permission_user_list = parseAccessControlList(*read_permission_user_list);
    logger_->log_debug("PutS3Object: Read Permission User List [{}]", *read_permission_user_list);
  }
  if (const auto read_acl_user_list = context.getProperty(ReadACLUserList, &flow_file)) {
    put_s3_request_params.read_acl_user_list = parseAccessControlList(*read_acl_user_list);
    logger_->log_debug("PutS3Object: Read ACL User List [{}]", *read_acl_user_list);
  }
  if (const auto write_acl_user_list = context.getProperty(WriteACLUserList, &flow_file)) {
    put_s3_request_params.write_acl_user_list = parseAccessControlList(*write_acl_user_list);
    logger_->log_debug("PutS3Object: Write ACL User List [{}]", *write_acl_user_list);
  }

  return setCannedAcl(context, flow_file, put_s3_request_params);
}

std::optional<aws::s3::PutObjectRequestParameters> PutS3Object::buildPutS3RequestParams(
    core::ProcessContext& context,
    const core::FlowFile& flow_file,
    const CommonProperties &common_properties) const {
  gsl_Expects(client_config_);
  aws::s3::PutObjectRequestParameters params(common_properties.credentials, *client_config_);
  params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  params.bucket = common_properties.bucket;
  params.user_metadata_map = user_metadata_map_;
  params.server_side_encryption = server_side_encryption_;
  params.storage_class = storage_class_;

  params.object_key = context.getProperty(ObjectKey, &flow_file).value_or("");
  if (params.object_key.empty() && (!flow_file.getAttribute("filename", params.object_key) || params.object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    return std::nullopt;
  }
  logger_->log_debug("PutS3Object: Object Key [{}]", params.object_key);

  params.content_type = context.getProperty(ContentType, &flow_file).value_or("");
  logger_->log_debug("PutS3Object: Content Type [{}]", params.content_type);

  if (!setAccessControl(context, flow_file, params)) {
    return std::nullopt;
  }

  params.use_virtual_addressing = use_virtual_addressing_;
  return params;
}

void PutS3Object::setAttributes(
    core::ProcessSession& session,
    core::FlowFile& flow_file,
    const aws::s3::PutObjectRequestParameters &put_s3_request_params,
    const minifi::aws::s3::PutObjectResult &put_object_result) const {
  session.putAttribute(flow_file, "s3.bucket", put_s3_request_params.bucket);
  session.putAttribute(flow_file, "s3.key", put_s3_request_params.object_key);
  session.putAttribute(flow_file, "s3.contenttype", put_s3_request_params.content_type);

  if (!user_metadata_.empty()) {
    session.putAttribute(flow_file, "s3.usermetadata", user_metadata_);
  }
  if (!put_object_result.version.empty()) {
    session.putAttribute(flow_file, "s3.version", put_object_result.version);
  }
  if (!put_object_result.etag.empty()) {
    session.putAttribute(flow_file, "s3.etag", put_object_result.etag);
  }
  if (!put_object_result.expiration.empty()) {
    session.putAttribute(flow_file, "s3.expiration", put_object_result.expiration);
  }
  if (!put_object_result.ssealgorithm.empty()) {
    session.putAttribute(flow_file, "s3.sseAlgorithm", put_object_result.ssealgorithm);
  }
}

void PutS3Object::ageOffMultipartUploads(const CommonProperties &common_properties) {
  {
    std::lock_guard<std::mutex> lock(last_ageoff_mutex_);
    const auto now = std::chrono::system_clock::now();
    if (now - last_ageoff_time_ < multipart_upload_ageoff_interval_) {
      logger_->log_debug("Multipart Upload Age off interval still in progress, not checking obsolete multipart uploads.");
      return;
    }
    last_ageoff_time_ = now;
  }

  logger_->log_trace("Listing aged off multipart uploads still in progress.");
  aws::s3::ListMultipartUploadsRequestParameters list_params(common_properties.credentials, *client_config_);
  list_params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
  list_params.bucket = common_properties.bucket;
  list_params.age_off_limit = multipart_upload_max_age_threshold_;
  list_params.use_virtual_addressing = use_virtual_addressing_;
  auto aged_off_uploads_in_progress = s3_wrapper_.listMultipartUploads(list_params);
  if (!aged_off_uploads_in_progress) {
    logger_->log_error("Listing aged off multipart uploads failed!");
    return;
  }

  logger_->log_info("Found {} aged off pending multipart upload jobs in bucket '{}'", aged_off_uploads_in_progress->size(), common_properties.bucket);
  size_t aborted = 0;
  for (const auto& upload : *aged_off_uploads_in_progress) {
    logger_->log_info("Aborting multipart upload with key '{}' and upload id '{}' in bucket '{}' due to reaching maximum upload age threshold.",
      upload.key, upload.upload_id, common_properties.bucket);
    aws::s3::AbortMultipartUploadRequestParameters abort_params(common_properties.credentials, *client_config_);
    abort_params.setClientConfig(common_properties.proxy, common_properties.endpoint_override_url);
    abort_params.bucket = common_properties.bucket;
    abort_params.key = upload.key;
    abort_params.upload_id = upload.upload_id;
    abort_params.use_virtual_addressing = use_virtual_addressing_;
    if (!s3_wrapper_.abortMultipartUpload(abort_params)) {
       logger_->log_error("Failed to abort multipart upload with key '{}' and upload id '{}' in bucket '{}'", abort_params.key, abort_params.upload_id, abort_params.bucket);
       continue;
    }
    ++aborted;
  }
  if (aborted > 0) {
    logger_->log_info("Aborted {} pending multipart upload jobs in bucket '{}'", aborted, common_properties.bucket);
  }
  s3_wrapper_.ageOffLocalS3MultipartUploadStates(multipart_upload_max_age_threshold_);
}

void PutS3Object::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutS3Object onTrigger");
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

  ageOffMultipartUploads(*common_properties);

  auto put_s3_request_params = buildPutS3RequestParams(context, *flow_file, *common_properties);
  if (!put_s3_request_params) {
    session.transfer(flow_file, Failure);
    return;
  }

  std::optional<minifi::aws::s3::PutObjectResult> result;
  session.read(flow_file, [this, &flow_file, &put_s3_request_params, &result](const std::shared_ptr<io::InputStream>& stream) -> int64_t {
    try {
      if (flow_file->getSize() <= multipart_threshold_) {
        logger_->log_info("Uploading S3 Object '{}' in a single upload", put_s3_request_params->object_key);
        result = s3_wrapper_.putObject(*put_s3_request_params, stream, flow_file->getSize());
        return gsl::narrow<int64_t>(flow_file->getSize());
      } else {
        logger_->log_info("S3 Object '{}' passes the multipart threshold, uploading it in multiple parts", put_s3_request_params->object_key);
        result = s3_wrapper_.putObjectMultipart(*put_s3_request_params, stream, flow_file->getSize(), multipart_size_);
        return gsl::narrow<int64_t>(flow_file->getSize());
      }
    } catch(const aws::s3::StreamReadException& ex) {
      logger_->log_error("Error occurred while uploading to S3: {}", ex.what());
      return -1;
    }
  });
  if (!result.has_value()) {
    logger_->log_error("Failed to upload S3 object to bucket '{}'", put_s3_request_params->bucket);
    session.transfer(flow_file, Failure);
  } else {
    setAttributes(session, *flow_file, *put_s3_request_params, *result);
    logger_->log_debug("Successfully uploaded S3 object '{}' to bucket '{}'", put_s3_request_params->object_key, put_s3_request_params->bucket);
    session.transfer(flow_file, Success);
  }
}

REGISTER_RESOURCE(PutS3Object, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
