/**
 * @file S3Wrapper.h
 * S3Wrapper class declaration
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

#include <optional>

#include "S3RequestSender.h"

namespace org::apache::nifi::minifi::aws::s3 {

class S3ClientRequestSender : public S3RequestSender {
 public:
  std::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(
    const Aws::S3::Model::PutObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  bool sendDeleteObjectRequest(
    const Aws::S3::Model::DeleteObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3::Model::GetObjectResult> sendGetObjectRequest(
    const Aws::S3::Model::GetObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3::Model::ListObjectsV2Result> sendListObjectsRequest(
    const Aws::S3::Model::ListObjectsV2Request& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3::Model::ListObjectVersionsResult> sendListVersionsRequest(
    const Aws::S3::Model::ListObjectVersionsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(
    const Aws::S3::Model::GetObjectTaggingRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3::Model::HeadObjectResult> sendHeadObjectRequest(
    const Aws::S3::Model::HeadObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3::Model::CreateMultipartUploadResult> sendCreateMultipartUploadRequest(
    const Aws::S3::Model::CreateMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  std::optional<Aws::S3::Model::UploadPartResult> sendUploadPartRequest(
    const Aws::S3::Model::UploadPartRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  std::optional<Aws::S3::Model::CompleteMultipartUploadResult> sendCompleteMultipartUploadRequest(
    const Aws::S3::Model::CompleteMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
};

}  // namespace org::apache::nifi::minifi::aws::s3
