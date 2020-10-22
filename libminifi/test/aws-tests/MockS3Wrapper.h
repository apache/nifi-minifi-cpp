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

#include "s3/S3WrapperBase.h"

const std::string S3_VERSION = "1.2.3";
const std::string S3_ETAG = "\"tag-123\"";
const std::string S3_ETAG_UNQUOTED = "tag-123";
const std::string S3_EXPIRATION = "expiry-date=\"Wed, 28 Oct 2020 00:00:00 GMT\", rule-id=\"my_expiration_rule\"";
const std::string S3_EXPIRATION_DATE = "Wed, 28 Oct 2020 00:00:00 GMT";
const Aws::S3::Model::ServerSideEncryption S3_SSEALGORITHM = Aws::S3::Model::ServerSideEncryption::aws_kms;
const std::string S3_SSEALGORITHM_STR = "aws_kms";

class MockS3Wrapper : public minifi::aws::s3::S3WrapperBase {
 public:
  Aws::Auth::AWSCredentials getCredentials() const {
    return credentials_;
  }

  Aws::Client::ClientConfiguration getClientConfig() const {
    return client_config_;
  }

  minifi::utils::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3::Model::PutObjectRequest& request) override {
    std::istreambuf_iterator<char> buf_it;
    put_s3_data = std::string(std::istreambuf_iterator<char>(*request.GetBody()), buf_it);
    bucket_name = request.GetBucket();
    object_key = request.GetKey();
    storage_class = request.GetStorageClass();
    server_side_encryption = request.GetServerSideEncryption();
    metadata_map = request.GetMetadata();
    content_type = request.GetContentType();
    fullcontrol_user_list = request.GetGrantFullControl();
    read_user_list = request.GetGrantRead();
    read_acl_user_list = request.GetGrantReadACP();
    write_acl_user_list = request.GetGrantWriteACP();
    write_acl_user_list = request.GetGrantWriteACP();
    canned_acl = request.GetACL();

    if (!get_empty_result) {
      put_s3_result.SetVersionId(S3_VERSION);
      put_s3_result.SetETag(S3_ETAG);
      put_s3_result.SetExpiration(S3_EXPIRATION);
      put_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
    }
    return put_s3_result;
  }

  bool sendDeleteObjectRequest(const Aws::S3::Model::DeleteObjectRequest& request) override {
    bucket_name = request.GetBucket();
    object_key = request.GetKey();
    version = request.GetVersionId();
    version_has_been_set = request.VersionIdHasBeenSet();

    return delete_object_result;
  }

  std::string bucket_name;
  std::string object_key;
  Aws::S3::Model::StorageClass storage_class;
  Aws::S3::Model::ServerSideEncryption server_side_encryption;
  Aws::S3::Model::PutObjectResult put_s3_result;
  std::string put_s3_data;
  std::map<std::string, std::string> metadata_map;
  std::string content_type;
  std::string fullcontrol_user_list;
  std::string read_user_list;
  std::string read_acl_user_list;
  std::string write_acl_user_list;
  Aws::S3::Model::ObjectCannedACL canned_acl;
  std::string version;
  bool version_has_been_set = false;
  bool get_empty_result = false;
  bool delete_object_result = true;
};
