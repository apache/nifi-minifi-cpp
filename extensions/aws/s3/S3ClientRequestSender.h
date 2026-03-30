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

#include <memory>
#include <mutex>
#include <optional>

#include "S3RequestSender.h"

namespace org::apache::nifi::minifi::aws::s3 {

class S3ClientRequestSender : public S3RequestSender {
 public:
  S3ClientRequestSender(const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing = true);
  std::optional<Aws::S3Crt::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3Crt::Model::PutObjectRequest& request) override;
  bool sendDeleteObjectRequest(const Aws::S3Crt::Model::DeleteObjectRequest& request) override;
  std::optional<Aws::S3Crt::Model::GetObjectResult> sendGetObjectRequest(const Aws::S3Crt::Model::GetObjectRequest& request) override;
  std::optional<Aws::S3Crt::Model::ListObjectsV2Result> sendListObjectsRequest(const Aws::S3Crt::Model::ListObjectsV2Request& request) override;
  std::optional<Aws::S3Crt::Model::ListObjectVersionsResult> sendListVersionsRequest(const Aws::S3Crt::Model::ListObjectVersionsRequest& request) override;
  std::optional<Aws::S3Crt::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(const Aws::S3Crt::Model::GetObjectTaggingRequest& request) override;
  std::optional<Aws::S3Crt::Model::HeadObjectResult> sendHeadObjectRequest(const Aws::S3Crt::Model::HeadObjectRequest& request) override;
  std::optional<Aws::S3Crt::Model::CreateMultipartUploadResult> sendCreateMultipartUploadRequest(const Aws::S3Crt::Model::CreateMultipartUploadRequest& request) override;
  std::optional<Aws::S3Crt::Model::UploadPartResult> sendUploadPartRequest(const Aws::S3Crt::Model::UploadPartRequest& request) override;
  std::optional<Aws::S3Crt::Model::CompleteMultipartUploadResult> sendCompleteMultipartUploadRequest(const Aws::S3Crt::Model::CompleteMultipartUploadRequest& request) override;
  std::optional<Aws::S3Crt::Model::ListMultipartUploadsResult> sendListMultipartUploadsRequest(const Aws::S3Crt::Model::ListMultipartUploadsRequest& request) override;
  bool sendAbortMultipartUploadRequest(const Aws::S3Crt::Model::AbortMultipartUploadRequest& request) override;

 private:
  Aws::S3Crt::S3CrtClient s3_client_;
};

}  // namespace org::apache::nifi::minifi::aws::s3
