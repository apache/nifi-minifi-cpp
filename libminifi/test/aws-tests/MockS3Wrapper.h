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

#include <string>
#include <sstream>
#include <vector>

#include "s3/S3WrapperBase.h"
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
const int64_t S3_OBJECT_AGE_MILLISECONDS = 1603989854123;
const std::string S3_STORAGE_CLASS_STR = "Standard";

class MockS3Wrapper : public minifi::aws::s3::S3WrapperBase {
 public:
  MockS3Wrapper() {
    for(auto i = 0; i < S3_OBJECT_COUNT; ++i) {
      Aws::S3::Model::ObjectVersion version;
      version.SetKey(S3_KEY_PREFIX + std::to_string(i));
      version.SetETag(S3_ETAG_PREFIX + std::to_string(i));
      version.SetIsLatest(false);
      version.SetStorageClass(Aws::S3::Model::ObjectVersionStorageClass::STANDARD);
      version.SetVersionId(S3_VERSION_1);
      version.SetSize(S3_OBJECT_SIZE);
      version.SetLastModified(Aws::Utils::DateTime(S3_OBJECT_AGE_MILLISECONDS));
      listed_versions_.push_back(version);
      version.SetVersionId(S3_VERSION_2);
      version.SetIsLatest(true);
      listed_versions_.push_back(version);
    }

    for(auto i = 0; i < S3_OBJECT_COUNT; ++i) {
      Aws::S3::Model::Object object;
      object.SetKey(S3_KEY_PREFIX + std::to_string(i));
      object.SetETag(S3_ETAG_PREFIX + std::to_string(i));
      object.SetStorageClass(Aws::S3::Model::ObjectStorageClass::STANDARD);
      object.SetSize(S3_OBJECT_SIZE);
      object.SetLastModified(Aws::Utils::DateTime(S3_OBJECT_AGE_MILLISECONDS));
      listed_objects_.push_back(object);
    }
  }

  minifi::utils::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3::Model::PutObjectRequest& request) override {
    put_object_request = request;

    Aws::S3::Model::PutObjectResult put_s3_result;
    if (!return_empty_result_) {
      put_s3_result.SetVersionId(S3_VERSION_1);
      put_s3_result.SetETag(S3_ETAG);
      put_s3_result.SetExpiration(S3_EXPIRATION);
      put_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
    }
    return put_s3_result;
  }

  bool sendDeleteObjectRequest(const Aws::S3::Model::DeleteObjectRequest& request) override {
    delete_object_request = request;
    return delete_object_result_;
  }

  minifi::utils::optional<Aws::S3::Model::GetObjectResult> sendGetObjectRequest(const Aws::S3::Model::GetObjectRequest& request) override {
    get_object_request = request;

    Aws::S3::Model::GetObjectResult get_s3_result;
    if (!return_empty_result_) {
      get_s3_result.SetVersionId(S3_VERSION_1);
      get_s3_result.SetETag(S3_ETAG);
      get_s3_result.SetExpiration(S3_EXPIRATION);
      get_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
      get_s3_result.SetContentType(S3_CONTENT_TYPE);
      get_s3_result.ReplaceBody(new std::stringstream(S3_CONTENT));
      get_s3_result.SetContentLength(S3_CONTENT.size());
    }
    return minifi::utils::make_optional(std::move(get_s3_result));
  }

  minifi::utils::optional<Aws::S3::Model::ListObjectsV2Result> sendListObjectsRequest(const Aws::S3::Model::ListObjectsV2Request& request) {
    list_object_request = request;

    Aws::S3::Model::ListObjectsV2Result list_object_result;
    if (request.GetContinuationToken().empty()) {
      list_object_result.SetContinuationToken("continue");
      list_object_result.SetIsTruncated(true);
      for (auto i = 0; i < listed_objects_.size() / 2; ++i) {
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

  minifi::utils::optional<Aws::S3::Model::ListObjectVersionsResult> sendListVersionsRequest(const Aws::S3::Model::ListObjectVersionsRequest& request) {
    list_version_request = request;

    Aws::S3::Model::ListObjectVersionsResult list_version_result;
    if (request.GetKeyMarker().empty() && request.GetVersionIdMarker().empty()) {
      list_version_result.SetKeyMarker("continue_key");
      list_version_result.SetNextVersionIdMarker("continue_version");
      list_version_result.SetIsTruncated(true);
      for (auto i = 0; i < listed_versions_.size() / 2; ++i) {
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

  Aws::Auth::AWSCredentials getCredentials() const {
    return credentials_;
  }

  Aws::Client::ClientConfiguration getClientConfig() const {
    return client_config_;
  }

  std::string getPutObjectRequestBody() const {
    std::istreambuf_iterator<char> buf_it;
    return std::string(std::istreambuf_iterator<char>(*put_object_request.GetBody()), buf_it);
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

  Aws::S3::Model::PutObjectRequest put_object_request;
  Aws::S3::Model::DeleteObjectRequest delete_object_request;
  Aws::S3::Model::GetObjectRequest get_object_request;
  Aws::S3::Model::ListObjectsV2Request list_object_request;
  Aws::S3::Model::ListObjectVersionsRequest list_version_request;

 private:
  std::vector<Aws::S3::Model::ObjectVersion> listed_versions_;
  std::vector<Aws::S3::Model::Object> listed_objects_;
  bool delete_object_result_ = true;
  bool return_empty_result_ = false;
};
