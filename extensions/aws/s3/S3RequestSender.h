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

#include <memory>
#include <optional>
#include <string>

#include "aws/core/auth/AWSCredentials.h"
#include "aws/core/client/ClientConfiguration.h"
#include "aws/core/utils/xml/XmlSerializer.h"
#include "aws/s3-crt/model/PutObjectRequest.h"
#include "aws/s3-crt/model/PutObjectResult.h"
#include "aws/s3-crt/model/DeleteObjectRequest.h"
#include "aws/s3-crt/model/GetObjectRequest.h"
#include "aws/s3-crt/model/GetObjectResult.h"
#include "aws/s3-crt/model/ListObjectsV2Request.h"
#include "aws/s3-crt/model/ListObjectsV2Result.h"
#include "aws/s3-crt/model/ListObjectVersionsRequest.h"
#include "aws/s3-crt/model/ListObjectVersionsResult.h"
#include "aws/s3-crt/model/GetObjectTaggingRequest.h"
#include "aws/s3-crt/model/GetObjectTaggingResult.h"
#include "aws/s3-crt/model/HeadObjectRequest.h"
#include "aws/s3-crt/model/HeadObjectResult.h"
#include "aws/s3-crt/model/CreateMultipartUploadRequest.h"
#include "aws/s3-crt/model/CreateMultipartUploadResult.h"
#include "aws/s3-crt/model/UploadPartRequest.h"
#include "aws/s3-crt/model/UploadPartResult.h"
#include "aws/s3-crt/model/CompleteMultipartUploadRequest.h"
#include "aws/s3-crt/model/CompleteMultipartUploadResult.h"
#include "aws/s3-crt/model/ListMultipartUploadsRequest.h"
#include "aws/s3-crt/model/ListMultipartUploadsResult.h"
#include "aws/s3-crt/model/AbortMultipartUploadRequest.h"
#include "aws/s3-crt/model/AbortMultipartUploadResult.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "utils/AWSInitializer.h"

namespace org::apache::nifi::minifi::aws::s3 {

class S3RequestSender {
 public:
  virtual std::optional<Aws::S3Crt::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3Crt::Model::PutObjectRequest& request) = 0;
  virtual bool sendDeleteObjectRequest(const Aws::S3Crt::Model::DeleteObjectRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::GetObjectResult> sendGetObjectRequest(const Aws::S3Crt::Model::GetObjectRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::ListObjectsV2Result> sendListObjectsRequest(const Aws::S3Crt::Model::ListObjectsV2Request& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::ListObjectVersionsResult> sendListVersionsRequest(const Aws::S3Crt::Model::ListObjectVersionsRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(const Aws::S3Crt::Model::GetObjectTaggingRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::HeadObjectResult> sendHeadObjectRequest(const Aws::S3Crt::Model::HeadObjectRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::CreateMultipartUploadResult> sendCreateMultipartUploadRequest(const Aws::S3Crt::Model::CreateMultipartUploadRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::UploadPartResult> sendUploadPartRequest(const Aws::S3Crt::Model::UploadPartRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::CompleteMultipartUploadResult> sendCompleteMultipartUploadRequest(const Aws::S3Crt::Model::CompleteMultipartUploadRequest& request) = 0;
  virtual std::optional<Aws::S3Crt::Model::ListMultipartUploadsResult> sendListMultipartUploadsRequest(const Aws::S3Crt::Model::ListMultipartUploadsRequest& request) = 0;
  virtual bool sendAbortMultipartUploadRequest(const Aws::S3Crt::Model::AbortMultipartUploadRequest& request) = 0;
  virtual ~S3RequestSender() = default;

 protected:
  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::shared_ptr<minifi::core::logging::Logger> logger_{minifi::core::logging::LoggerFactory<S3RequestSender>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::aws::s3
