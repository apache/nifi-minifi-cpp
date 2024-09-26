/**
 * @file S3RequestSender.h
 * S3RequestSender class declaration
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

#include <memory>
#include <optional>
#include <string>

#include "aws/core/auth/AWSCredentials.h"
#include "aws/core/client/ClientConfiguration.h"
#include "aws/core/utils/xml/XmlSerializer.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/PutObjectResult.h"
#include "aws/s3/model/DeleteObjectRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/GetObjectResult.h"
#include "aws/s3/model/ListObjectsV2Request.h"
#include "aws/s3/model/ListObjectsV2Result.h"
#include "aws/s3/model/ListObjectVersionsRequest.h"
#include "aws/s3/model/ListObjectVersionsResult.h"
#include "aws/s3/model/GetObjectTaggingRequest.h"
#include "aws/s3/model/GetObjectTaggingResult.h"
#include "aws/s3/model/HeadObjectRequest.h"
#include "aws/s3/model/HeadObjectResult.h"
#include "aws/s3/model/CreateMultipartUploadRequest.h"
#include "aws/s3/model/CreateMultipartUploadResult.h"
#include "aws/s3/model/UploadPartRequest.h"
#include "aws/s3/model/UploadPartResult.h"
#include "aws/s3/model/CompleteMultipartUploadRequest.h"
#include "aws/s3/model/CompleteMultipartUploadResult.h"
#include "aws/s3/model/ListMultipartUploadsRequest.h"
#include "aws/s3/model/ListMultipartUploadsResult.h"
#include "aws/s3/model/AbortMultipartUploadRequest.h"
#include "aws/s3/model/AbortMultipartUploadResult.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/AWSInitializer.h"

namespace org::apache::nifi::minifi::aws::s3 {

struct ProxyOptions {
  std::string host;
  uint32_t port = 0;
  std::string username;
  std::string password;
};

class S3RequestSender {
 public:
  virtual std::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(
    const Aws::S3::Model::PutObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) = 0;
  virtual bool sendDeleteObjectRequest(
    const Aws::S3::Model::DeleteObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) = 0;
  virtual std::optional<Aws::S3::Model::GetObjectResult> sendGetObjectRequest(
    const Aws::S3::Model::GetObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) = 0;
  virtual std::optional<Aws::S3::Model::ListObjectsV2Result> sendListObjectsRequest(
    const Aws::S3::Model::ListObjectsV2Request& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) = 0;
  virtual std::optional<Aws::S3::Model::ListObjectVersionsResult> sendListVersionsRequest(
    const Aws::S3::Model::ListObjectVersionsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) = 0;
  virtual std::optional<Aws::S3::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(
    const Aws::S3::Model::GetObjectTaggingRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) = 0;
  virtual std::optional<Aws::S3::Model::HeadObjectResult> sendHeadObjectRequest(
    const Aws::S3::Model::HeadObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) = 0;
  virtual std::optional<Aws::S3::Model::CreateMultipartUploadResult> sendCreateMultipartUploadRequest(
    const Aws::S3::Model::CreateMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) = 0;
  virtual std::optional<Aws::S3::Model::UploadPartResult> sendUploadPartRequest(
    const Aws::S3::Model::UploadPartRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) = 0;
  virtual std::optional<Aws::S3::Model::CompleteMultipartUploadResult> sendCompleteMultipartUploadRequest(
    const Aws::S3::Model::CompleteMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) = 0;
  virtual std::optional<Aws::S3::Model::ListMultipartUploadsResult> sendListMultipartUploadsRequest(
    const Aws::S3::Model::ListMultipartUploadsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) = 0;
  virtual bool sendAbortMultipartUploadRequest(
    const Aws::S3::Model::AbortMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) = 0;
  virtual ~S3RequestSender() = default;

 protected:
  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::shared_ptr<minifi::core::logging::Logger> logger_{minifi::core::logging::LoggerFactory<S3RequestSender>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::aws::s3
