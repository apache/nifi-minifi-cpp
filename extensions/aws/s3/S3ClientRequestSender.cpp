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
#include "S3ClientRequestSender.h"

#include <mutex>

#include <aws/s3-crt/S3CrtClient.h>

namespace org::apache::nifi::minifi::aws::s3 {

// Default destructor: members are destroyed in reverse declaration order
// (s3_client_ → client_bootstrap_ → host_resolver_ → event_loop_group_),
// which ensures the CRT client shuts down while the dedicated event loop
// is still alive to process the async shutdown callback.
S3ClientRequestSender::~S3ClientRequestSender() = default;

std::shared_ptr<Aws::S3Crt::S3CrtClient> S3ClientRequestSender::getOrCreateClient(
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  std::lock_guard<std::mutex> lock(client_mutex_);
  if (s3_client_ &&
      cached_access_key_id_ == credentials.GetAWSAccessKeyId() &&
      cached_secret_key_ == credentials.GetAWSSecretKey() &&
      cached_session_token_ == credentials.GetSessionToken() &&
      cached_region_ == client_config.region &&
      cached_endpoint_ == client_config.endpointOverride &&
      cached_use_virtual_addressing_ == use_virtual_addressing) {
    return s3_client_;
  }

  // Destroy the old client before creating new infrastructure
  s3_client_.reset();

  Aws::S3Crt::ClientConfiguration s3_crt_config(client_config);
  s3_crt_config.useVirtualAddressing = use_virtual_addressing;

  // S3 Express One Zone uses an identity provider that holds a back-reference
  // to the S3CrtClient, which can prevent the CRT's async shutdown from
  // completing.  Disable it unless explicitly needed.
  s3_crt_config.disableS3ExpressAuth = true;

  const double throughput_target_gbps = 5;
  const uint64_t part_size = 20 * 1024 * 1024; // 20 MB.

  s3_crt_config.throughputTargetGbps = throughput_target_gbps;
  s3_crt_config.partSize = part_size;

  // Create a dedicated event loop group and client bootstrap so the CRT client
  // manages its own event loop lifecycle and shuts down cleanly.
  event_loop_group_ = Aws::MakeShared<Aws::Crt::Io::EventLoopGroup>("S3CrtRequestSender", 1);
  host_resolver_ = Aws::MakeShared<Aws::Crt::Io::DefaultHostResolver>("S3CrtRequestSender", *event_loop_group_, 8, 30);
  client_bootstrap_ = Aws::MakeShared<Aws::Crt::Io::ClientBootstrap>("S3CrtRequestSender", *event_loop_group_, *host_resolver_);
  s3_crt_config.clientBootstrap = client_bootstrap_;

  s3_client_ = std::make_shared<Aws::S3Crt::S3CrtClient>(credentials, s3_crt_config);

  cached_access_key_id_ = credentials.GetAWSAccessKeyId();
  cached_secret_key_ = credentials.GetAWSSecretKey();
  cached_session_token_ = credentials.GetSessionToken();
  cached_region_ = client_config.region;
  cached_endpoint_ = client_config.endpointOverride;
  cached_use_virtual_addressing_ = use_virtual_addressing;

  return s3_client_;
}

std::optional<Aws::S3Crt::Model::PutObjectResult> S3ClientRequestSender::sendPutObjectRequest(
    const Aws::S3Crt::Model::PutObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  auto s3_client = getOrCreateClient(credentials, client_config, use_virtual_addressing);
  auto outcome = s3_client->PutObject(request);

  if (outcome.IsSuccess()) {
      logger_->log_debug("Added S3 object '{}' to bucket '{}'", request.GetKey(), request.GetBucket());
      return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("PutS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

bool S3ClientRequestSender::sendDeleteObjectRequest(
    const Aws::S3Crt::Model::DeleteObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  auto s3_client = getOrCreateClient(credentials, client_config);
  Aws::S3Crt::Model::DeleteObjectOutcome outcome = s3_client->DeleteObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Deleted S3 object '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return true;
  } else if (outcome.GetError().GetErrorType() == Aws::S3Crt::S3CrtErrors::NO_SUCH_KEY) {
    logger_->log_debug("S3 object '{}' was not found in bucket '{}'", request.GetKey(), request.GetBucket());
    return true;
  } else {
    logger_->log_error("DeleteS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return false;
  }
}

std::optional<Aws::S3Crt::Model::GetObjectResult> S3ClientRequestSender::sendGetObjectRequest(
    const Aws::S3Crt::Model::GetObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  auto s3_client = getOrCreateClient(credentials, client_config);
  auto outcome = s3_client->GetObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Fetched S3 object '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("FetchS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::ListObjectsV2Result> S3ClientRequestSender::sendListObjectsRequest(
    const Aws::S3Crt::Model::ListObjectsV2Request& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  auto s3_client = getOrCreateClient(credentials, client_config);
  auto outcome = s3_client->ListObjectsV2(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListObjectsV2 successful of bucket '{}'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectsV2 failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::ListObjectVersionsResult> S3ClientRequestSender::sendListVersionsRequest(
    const Aws::S3Crt::Model::ListObjectVersionsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  auto s3_client = getOrCreateClient(credentials, client_config);
  auto outcome = s3_client->ListObjectVersions(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListObjectVersions successful of bucket '{}'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectVersions failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::GetObjectTaggingResult> S3ClientRequestSender::sendGetObjectTaggingRequest(
    const Aws::S3Crt::Model::GetObjectTaggingRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  auto s3_client = getOrCreateClient(credentials, client_config);
  auto outcome = s3_client->GetObjectTagging(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Got tags for S3 object '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("GetObjectTagging failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::HeadObjectResult> S3ClientRequestSender::sendHeadObjectRequest(
    const Aws::S3Crt::Model::HeadObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  auto s3_client = getOrCreateClient(credentials, client_config);
  auto outcome = s3_client->HeadObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("HeadS3Object successful for key '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("HeadS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::CreateMultipartUploadResult> S3ClientRequestSender::sendCreateMultipartUploadRequest(
    const Aws::S3Crt::Model::CreateMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  auto s3_client = getOrCreateClient(credentials, client_config, use_virtual_addressing);
  auto outcome = s3_client->CreateMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("CreateMultipartUpload successful for key '{}' and bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("CreateMultipartUpload failed for key '{}' and bucket '{}' with the following: '{}'", request.GetKey(), request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::UploadPartResult> S3ClientRequestSender::sendUploadPartRequest(
    const Aws::S3Crt::Model::UploadPartRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  auto s3_client = getOrCreateClient(credentials, client_config, use_virtual_addressing);
  auto outcome = s3_client->UploadPart(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("UploadPart successful for key '{}' from bucket '{}' with part number {}", request.GetKey(), request.GetBucket(), request.GetPartNumber());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("UploadPart failed for key '{}' from bucket '{}' with part number {} with the following: '{}'",
      request.GetKey(), request.GetBucket(), request.GetPartNumber(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::CompleteMultipartUploadResult> S3ClientRequestSender::sendCompleteMultipartUploadRequest(
    const Aws::S3Crt::Model::CompleteMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  auto s3_client = getOrCreateClient(credentials, client_config, use_virtual_addressing);
  auto outcome = s3_client->CompleteMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("CompleteMultipartUpload successful for key '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("CompleteMultipartUpload failed for key '{}' from bucket '{}' with the following: '{}'", request.GetKey(), request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::ListMultipartUploadsResult> S3ClientRequestSender::sendListMultipartUploadsRequest(
    const Aws::S3Crt::Model::ListMultipartUploadsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  auto s3_client = getOrCreateClient(credentials, client_config, use_virtual_addressing);
  auto outcome = s3_client->ListMultipartUploads(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListMultipartUploads successful for bucket '{}'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListMultipartUploads failed for bucket '{}' with the following: '{}'", request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

bool S3ClientRequestSender::sendAbortMultipartUploadRequest(
    const Aws::S3Crt::Model::AbortMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  auto s3_client = getOrCreateClient(credentials, client_config, use_virtual_addressing);
  auto outcome = s3_client->AbortMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("AbortMultipartUpload successful for bucket '{}', key '{}', upload id '{}'", request.GetBucket(), request.GetKey(), request.GetUploadId());
    return true;
  } else {
    logger_->log_error("AbortMultipartUpload failed for bucket '{}', key '{}', upload id '{}' with the following: '{}'",
      request.GetBucket(), request.GetKey(), request.GetUploadId(), outcome.GetError().GetMessage());
    return false;
  }
}

}  // namespace org::apache::nifi::minifi::aws::s3
