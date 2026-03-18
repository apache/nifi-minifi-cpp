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
#include <mutex>
#include <optional>

#include <aws/core/auth/AWSCredentials.h>
#include <aws/crt/io/Bootstrap.h>
#include <aws/crt/io/EventLoopGroup.h>
#include <aws/crt/io/HostResolver.h>
#include <aws/s3-crt/S3CrtClient.h>

#include "S3RequestSender.h"

namespace org::apache::nifi::minifi::aws::s3 {

class S3ClientRequestSender : public S3RequestSender {
 public:
  ~S3ClientRequestSender() override;

  std::optional<Aws::S3Crt::Model::PutObjectResult> sendPutObjectRequest(
    const Aws::S3Crt::Model::PutObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  bool sendDeleteObjectRequest(
    const Aws::S3Crt::Model::DeleteObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3Crt::Model::GetObjectResult> sendGetObjectRequest(
    const Aws::S3Crt::Model::GetObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3Crt::Model::ListObjectsV2Result> sendListObjectsRequest(
    const Aws::S3Crt::Model::ListObjectsV2Request& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3Crt::Model::ListObjectVersionsResult> sendListVersionsRequest(
    const Aws::S3Crt::Model::ListObjectVersionsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3Crt::Model::GetObjectTaggingResult> sendGetObjectTaggingRequest(
    const Aws::S3Crt::Model::GetObjectTaggingRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3Crt::Model::HeadObjectResult> sendHeadObjectRequest(
    const Aws::S3Crt::Model::HeadObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) override;
  std::optional<Aws::S3Crt::Model::CreateMultipartUploadResult> sendCreateMultipartUploadRequest(
    const Aws::S3Crt::Model::CreateMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  std::optional<Aws::S3Crt::Model::UploadPartResult> sendUploadPartRequest(
    const Aws::S3Crt::Model::UploadPartRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  std::optional<Aws::S3Crt::Model::CompleteMultipartUploadResult> sendCompleteMultipartUploadRequest(
    const Aws::S3Crt::Model::CompleteMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  std::optional<Aws::S3Crt::Model::ListMultipartUploadsResult> sendListMultipartUploadsRequest(
    const Aws::S3Crt::Model::ListMultipartUploadsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;
  bool sendAbortMultipartUploadRequest(
    const Aws::S3Crt::Model::AbortMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) override;

 private:
  std::shared_ptr<Aws::S3Crt::S3CrtClient> getOrCreateClient(
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing = true);

  // Destruction order matters: s3_client_ must be destroyed before the bootstrap/event loop.
  // Members are destroyed in reverse declaration order, so declare infrastructure first.
  std::shared_ptr<Aws::Crt::Io::EventLoopGroup> event_loop_group_;
  std::shared_ptr<Aws::Crt::Io::DefaultHostResolver> host_resolver_;
  std::shared_ptr<Aws::Crt::Io::ClientBootstrap> client_bootstrap_;
  std::shared_ptr<Aws::S3Crt::S3CrtClient> s3_client_;
  std::mutex client_mutex_;
  Aws::String cached_access_key_id_;
  Aws::String cached_secret_key_;
  Aws::String cached_session_token_;
  Aws::String cached_region_;
  Aws::String cached_endpoint_;
  bool cached_use_virtual_addressing_ = true;
};

}  // namespace org::apache::nifi::minifi::aws::s3
