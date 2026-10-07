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

#include <aws/core/auth/AWSCredentials.h>
#include <aws/crt/io/Bootstrap.h>
#include <aws/crt/io/EventLoopGroup.h>
#include <aws/crt/io/HostResolver.h>
#include <aws/s3-crt/S3CrtClient.h>
#include <aws/s3-crt/S3CrtErrors.h>

#include <memory>
#include <mutex>

#include "S3RequestSender.h"

namespace org::apache::nifi::minifi::aws::s3 {

class S3ClientRequestSender : public S3RequestSender {
 public:
  S3ClientRequestSender(const Aws::Auth::AWSCredentials& credentials, const Aws::Client::ClientConfiguration& client_config,
      bool use_virtual_addressing = true);
  [[nodiscard]] std::expected<Aws::S3Crt::Model::PutObjectResult, S3Error> sendPutObjectRequest(const Aws::S3Crt::Model::PutObjectRequest& request) override;
  [[nodiscard]] std::expected<void, S3Error> sendDeleteObjectRequest(const Aws::S3Crt::Model::DeleteObjectRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::GetObjectResult, S3Error> sendGetObjectRequest(const Aws::S3Crt::Model::GetObjectRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::ListObjectsV2Result, S3Error> sendListObjectsRequest(const Aws::S3Crt::Model::ListObjectsV2Request& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::ListObjectVersionsResult, S3Error> sendListVersionsRequest(const Aws::S3Crt::Model::ListObjectVersionsRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::GetObjectTaggingResult, S3Error> sendGetObjectTaggingRequest(const Aws::S3Crt::Model::GetObjectTaggingRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::HeadObjectResult, S3Error> sendHeadObjectRequest(const Aws::S3Crt::Model::HeadObjectRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::CreateMultipartUploadResult, S3Error> sendCreateMultipartUploadRequest(const Aws::S3Crt::Model::CreateMultipartUploadRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::UploadPartResult, S3Error> sendUploadPartRequest(const Aws::S3Crt::Model::UploadPartRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::CompleteMultipartUploadResult, S3Error> sendCompleteMultipartUploadRequest(const Aws::S3Crt::Model::CompleteMultipartUploadRequest& request) override;
  [[nodiscard]] std::expected<Aws::S3Crt::Model::ListMultipartUploadsResult, S3Error> sendListMultipartUploadsRequest(const Aws::S3Crt::Model::ListMultipartUploadsRequest& request) override;
  [[nodiscard]] std::expected<void, S3Error> sendAbortMultipartUploadRequest(const Aws::S3Crt::Model::AbortMultipartUploadRequest& request) override;

 private:
  Aws::S3Crt::S3CrtClient s3_client_;
};

}  // namespace org::apache::nifi::minifi::aws::s3
