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
  minifi::utils::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3::Model::PutObjectRequest& request) override {
    put_object_request = request;

    Aws::S3::Model::PutObjectResult put_s3_result;
    if (!return_empty_result_) {
      put_s3_result.SetVersionId(S3_VERSION);
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

  Aws::S3::Model::PutObjectRequest put_object_request;
  Aws::S3::Model::DeleteObjectRequest delete_object_request;

 private:
  bool delete_object_result_ = true;
  bool return_empty_result_ = false;
};
