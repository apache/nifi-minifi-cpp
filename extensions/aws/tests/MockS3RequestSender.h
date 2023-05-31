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

#pragma once

#include <map>
#include <optional>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "s3/S3RequestSender.h"
#include "aws/core/utils/DateTime.h"

const std::string S3_VERSION_1 = "1.2.3";
const std::string S3_VERSION_2 = "1.2.4";
const std::string S3_ETAG = "\"tag-123\"";
const std::string S3_ETAG_UNQUOTED = "tag-123";
const std::string S3_EXPIRATION = "expiry-date=\"Wed, 28 Oct 2020 00:00:00 GMT\", rule-id=\"my_expiration_rule\"";
const std::string S3_EXPIRATION_DATE = "Wed, 28 Oct 2020 00:00:00 GMT";
const std::string S3_EXPIRATION_TIME_RULE_ID = "my_expiration_rule";
const Aws::S3::Model::ServerSideEncryption S3_SSEALGORITHM = Aws::S3::Model::ServerSideEncryption::aws_kms;
const std::string S3_SSEALGORITHM_STR = "aws_kms";
const std::string S3_CONTENT_TYPE = "application/octet-stream";
const std::string S3_CONTENT = "INPUT_DATA";
const std::string S3_KEY_PREFIX = "KEY_";
const std::string S3_ETAG_PREFIX = "ETAG_";
const std::size_t S3_OBJECT_COUNT = 10;
const int64_t S3_OBJECT_SIZE = 1024;
const int64_t S3_OBJECT_OLD_AGE_MILLISECONDS = 652924800;
const std::string S3_STORAGE_CLASS_STR = "Standard";
const std::map<std::string, std::string> S3_OBJECT_TAGS {
  std::make_pair("tag1", "value1"),
  std::make_pair("tag2", "value2")
};
const std::map<std::string, std::string> S3_OBJECT_USER_METADATA {
  std::make_pair("metadata_key_1", "metadata_value_1"),
  std::make_pair("metadata_key_2", "metadata_value_2")
};
const std::string S3_KEY_MARKER = "continue_key";
const std::string S3_VERSION_ID_MARKER = "continue_version";
const std::string S3_CONTINUATION_TOKEN = "continue";
const std::string S3_UPLOAD_ID = "test_upload_id";

class MockS3RequestSender : public minifi::aws::s3::S3RequestSender {
 public:
  MockS3RequestSender() {
    for (std::size_t i = 0; i < S3_OBJECT_COUNT; ++i) {
      Aws::S3::Model::ObjectVersion version;
      version.SetKey(S3_KEY_PREFIX + std::to_string(i));
      version.SetETag(S3_ETAG_PREFIX + std::to_string(i));
      version.SetIsLatest(false);
      version.SetStorageClass(Aws::S3::Model::ObjectVersionStorageClass::STANDARD);
      version.SetVersionId(S3_VERSION_1);
      version.SetSize(S3_OBJECT_SIZE);
      version.SetLastModified(Aws::Utils::DateTime(S3_OBJECT_OLD_AGE_MILLISECONDS));
      listed_versions_.push_back(version);
      version.SetVersionId(S3_VERSION_2);
      version.SetIsLatest(true);
      version.SetLastModified(Aws::Utils::DateTime::CurrentTimeMillis());
      listed_versions_.push_back(version);
    }

    for (std::size_t i = 0; i < S3_OBJECT_COUNT; ++i) {
      Aws::S3::Model::Object object;
      object.SetKey(S3_KEY_PREFIX + std::to_string(i));
      object.SetETag(S3_ETAG_PREFIX + std::to_string(i));
      object.SetStorageClass(Aws::S3::Model::ObjectStorageClass::STANDARD);
      object.SetSize(S3_OBJECT_SIZE);
      if (i % 2 == 0) {
        object.SetLastModified(Aws::Utils::DateTime(S3_OBJECT_OLD_AGE_MILLISECONDS));
      } else {
        object.SetLastModified(Aws::Utils::DateTime::CurrentTimeMillis());
      }
      listed_objects_.push_back(object);
    }
  }

  std::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(
      const Aws::S3::Model::PutObjectRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing) override {
    put_object_request = request;
    credentials_ = credentials;
    client_config_ = client_config;
    use_virtual_addressing_ = use_virtual_addressing;

    Aws::S3::Model::PutObjectResult put_s3_result;
    if (!return_empty_result_) {
      put_s3_result.SetVersionId(S3_VERSION_1);
      put_s3_result.SetETag(S3_ETAG);
      put_s3_result.SetExpiration(S3_EXPIRATION);
      put_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
    }
    return put_s3_result;
  }

  bool sendDeleteObjectRequest(
      const Aws::S3::Model::DeleteObjectRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config) override {
    delete_object_request = request;
    credentials_ = credentials;
    client_config_ = client_config;
    return delete_object_result_;
  }

  std::optional<Aws::S3::Model::GetObjectResult> sendGetObjectRequest(
      const Aws::S3::Model::GetObjectRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config) override {
    get_object_request = request;
    credentials_ = credentials;
    client_config_ = client_config;

    Aws::S3::Model::GetObjectResult get_s3_result;
    if (!return_empty_result_) {
      get_s3_result.SetVersionId(S3_VERSION_1);
      get_s3_result.SetETag(S3_ETAG);
      get_s3_result.SetExpiration(S3_EXPIRATION);
      get_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
      get_s3_result.SetContentType(S3_CONTENT_TYPE);
      get_s3_result.ReplaceBody(new std::stringstream(S3_CONTENT));
      get_s3_result.SetContentLength(S3_CONTENT.size());
      get_s3_result.SetMetadata(S3_OBJECT_USER_METADATA);
    }
    return std::make_optional(std::move(get_s3_result));
  }

  std::optional<Aws::S3::Model::ListObjectsV2Result> sendListObjectsRequest(
      const Aws::S3::Model::ListObjectsV2Request& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config) override {
    list_object_request = request;
    credentials_ = credentials;
    client_config_ = client_config;

    Aws::S3::Model::ListObjectsV2Result list_object_result;
    if (!is_listing_truncated_) {
      for (std::size_t i = 0; i < listed_objects_.size(); ++i) {
        list_object_result.AddContents(listed_objects_[i]);
      }
      return list_object_result;
    }

    if (request.GetContinuationToken().empty()) {
      list_object_result.SetNextContinuationToken(S3_CONTINUATION_TOKEN);
      list_object_result.SetIsTruncated(true);
      for (std::size_t i = 0; i < listed_objects_.size() / 2; ++i) {
        list_object_result.AddContents(listed_objects_[i]);
      }
    } else {
      list_object_result.SetIsTruncated(false);
      for (auto i = listed_objects_.size() / 2; i < listed_objects_.size(); ++i) {
        list_object_result.AddContents(listed_objects_[i]);
      }
    }
    return list_object_result;
  }

  std::optional<Aws::S3::Model::ListObjectVersionsResult> sendListVersionsRequest(
      const Aws::S3::Model::ListObjectVersionsRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config) override {
    list_version_request = request;
    credentials_ = credentials;
    client_config_ = client_config;

    Aws::S3::Model::ListObjectVersionsResult list_version_result;
    if (!is_listing_truncated_) {
      for (std::size_t i = 0; i < listed_versions_.size(); ++i) {
        list_version_result.AddVersions(listed_versions_[i]);
      }
      return list_version_result;
    }

    if (request.GetKeyMarker().empty() && request.GetVersionIdMarker().empty()) {
      list_version_result.SetNextKeyMarker(S3_KEY_MARKER);
      list_version_result.SetNextVersionIdMarker(S3_VERSION_ID_MARKER);
      list_version_result.SetIsTruncated(true);
      for (std::size_t i = 0; i < listed_versions_.size() / 2; ++i) {
        list_version_result.AddVersions(listed_versions_[i]);
      }
    } else {
      list_version_result.SetIsTruncated(false);
      for (auto i = listed_versions_.size() / 2; i < listed_versions_.size(); ++i) {
        list_version_result.AddVersions(listed_versions_[i]);
      }
    }
    return list_version_result;
  }

  std::optional<Aws::S3::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(
      const Aws::S3::Model::GetObjectTaggingRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config) override {
    get_object_tagging_request = request;
    credentials_ = credentials;
    client_config_ = client_config;
    Aws::S3::Model::GetObjectTaggingResult result;
    for (const auto& tag_pair : S3_OBJECT_TAGS) {
      Aws::S3::Model::Tag tag;
      tag.SetKey(tag_pair.first);
      tag.SetValue(tag_pair.second);
      result.AddTagSet(tag);
    }
    return result;
  }

  std::optional<Aws::S3::Model::HeadObjectResult> sendHeadObjectRequest(
      const Aws::S3::Model::HeadObjectRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config) override {
    head_object_request = request;
    credentials_ = credentials;
    client_config_ = client_config;

    Aws::S3::Model::HeadObjectResult head_s3_result;
    if (!return_empty_result_) {
      head_s3_result.SetVersionId(S3_VERSION_1);
      head_s3_result.SetETag(S3_ETAG);
      head_s3_result.SetExpiration(S3_EXPIRATION);
      head_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
      head_s3_result.SetContentType(S3_CONTENT_TYPE);
      head_s3_result.SetContentLength(S3_CONTENT.size());
      head_s3_result.SetMetadata(S3_OBJECT_USER_METADATA);
    }
    return std::make_optional(std::move(head_s3_result));
  }

  std::optional<Aws::S3::Model::CreateMultipartUploadResult> sendCreateMultipartUploadRequest(
      const Aws::S3::Model::CreateMultipartUploadRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing) override {
    create_multipart_upload_request = request;
    credentials_ = credentials;
    client_config_ = client_config;
    use_virtual_addressing_ = use_virtual_addressing;
    Aws::S3::Model::CreateMultipartUploadResult result;
    result.SetUploadId(S3_UPLOAD_ID);
    return std::make_optional(std::move(result));
  }

  std::optional<Aws::S3::Model::UploadPartResult> sendUploadPartRequest(
      const Aws::S3::Model::UploadPartRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing) override {
    upload_part_requests.push_back(request);
    credentials_ = credentials;
    client_config_ = client_config;
    use_virtual_addressing_ = use_virtual_addressing;
    Aws::S3::Model::UploadPartResult result;
    result.SetETag("etag" + std::to_string(etag_counter_));
    ++etag_counter_;
    return std::make_optional(std::move(result));
  }

  std::optional<Aws::S3::Model::CompleteMultipartUploadResult> sendCompleteMultipartUploadRequest(
      const Aws::S3::Model::CompleteMultipartUploadRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing) override {
    complete_multipart_upload_request = request;
    credentials_ = credentials;
    client_config_ = client_config;
    use_virtual_addressing_ = use_virtual_addressing;
    Aws::S3::Model::CompleteMultipartUploadResult result;
    if (!return_empty_result_) {
      result.SetVersionId(S3_VERSION_1);
      result.SetETag(S3_ETAG);
      result.SetExpiration(S3_EXPIRATION);
      result.SetServerSideEncryption(S3_SSEALGORITHM);
    }
    return std::make_optional(std::move(result));
  }

  std::optional<Aws::S3::Model::ListMultipartUploadsResult> sendListMultipartUploadsRequest(
      const Aws::S3::Model::ListMultipartUploadsRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing) override {
    list_multipart_upload_request = request;
    credentials_ = credentials;
    client_config_ = client_config;
    use_virtual_addressing_ = use_virtual_addressing;
    Aws::S3::Model::ListMultipartUploadsResult result;
    Aws::Vector<Aws::S3::Model::MultipartUpload> uploads;
    Aws::S3::Model::MultipartUpload upload1;
    upload1.SetKey("recent_key");
    upload1.SetUploadId("upload1");
    upload1.SetInitiated(Aws::Utils::DateTime::CurrentTimeMillis());
    uploads.push_back(upload1);

    Aws::S3::Model::MultipartUpload upload2;
    upload2.SetKey("old_key");
    upload2.SetUploadId("upload2");
    upload2.SetInitiated(Aws::Utils::DateTime("1980-05-31T15:55:55Z", Aws::Utils::DateFormat::AutoDetect));
    uploads.push_back(upload2);
    result.SetUploads(uploads);
    return std::make_optional(std::move(result));
  }

  bool sendAbortMultipartUploadRequest(
      const Aws::S3::Model::AbortMultipartUploadRequest& request,
      const Aws::Auth::AWSCredentials& credentials,
      const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing) override {
    abort_multipart_upload_requests.push_back(request);
    credentials_ = credentials;
    client_config_ = client_config;
    use_virtual_addressing_ = use_virtual_addressing;
    Aws::S3::Model::AbortMultipartUploadResult result;
    return true;
  }

  Aws::Auth::AWSCredentials getCredentials() const {
    return credentials_;
  }

  Aws::Client::ClientConfiguration getClientConfig() const {
    return client_config_;
  }

  bool getUseVirtualAddressing() const {
    return use_virtual_addressing_;
  }

  std::string getPutObjectRequestBody() const {
    std::istreambuf_iterator<char> buf_it;
    return std::string(std::istreambuf_iterator<char>(*put_object_request.GetBody()), buf_it);
  }

  static std::string getUploadPartRequestBody(const Aws::S3::Model::UploadPartRequest& upload_part_request) {
    std::istreambuf_iterator<char> buf_it;
    return std::string(std::istreambuf_iterator<char>(*upload_part_request.GetBody()), buf_it);
  }

  void returnEmptyS3Result(bool return_empty_result = true) {
    return_empty_result_ = return_empty_result;
  }

  void setDeleteObjectResult(bool delete_object_result) {
    delete_object_result_ = delete_object_result;
  }

  std::vector<Aws::S3::Model::ObjectVersion> getListedVersion() const {
    return listed_versions_;
  }

  std::vector<Aws::S3::Model::Object> getListedObjects() const {
    return listed_objects_;
  }

  void setListingTruncated(bool is_listing_truncated) {
    is_listing_truncated_ = is_listing_truncated;
  }

  Aws::S3::Model::PutObjectRequest put_object_request;
  Aws::S3::Model::DeleteObjectRequest delete_object_request;
  Aws::S3::Model::GetObjectRequest get_object_request;
  Aws::S3::Model::ListObjectsV2Request list_object_request;
  Aws::S3::Model::ListObjectVersionsRequest list_version_request;
  Aws::S3::Model::GetObjectTaggingRequest get_object_tagging_request;
  Aws::S3::Model::HeadObjectRequest head_object_request;
  Aws::S3::Model::CreateMultipartUploadRequest create_multipart_upload_request;
  std::vector<Aws::S3::Model::UploadPartRequest> upload_part_requests;
  Aws::S3::Model::CompleteMultipartUploadRequest complete_multipart_upload_request;
  Aws::S3::Model::ListMultipartUploadsRequest list_multipart_upload_request;
  std::vector<Aws::S3::Model::AbortMultipartUploadRequest> abort_multipart_upload_requests;

 private:
  std::vector<Aws::S3::Model::ObjectVersion> listed_versions_;
  std::vector<Aws::S3::Model::Object> listed_objects_;
  bool delete_object_result_ = true;
  bool return_empty_result_ = false;
  bool is_listing_truncated_ = false;
  Aws::Auth::AWSCredentials credentials_;
  Aws::Client::ClientConfiguration client_config_;
  bool use_virtual_addressing_ = true;
  uint32_t etag_counter_ = 0;
};
