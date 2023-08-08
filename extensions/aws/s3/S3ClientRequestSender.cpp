/**
 * @file S3ClientRequestSender.cpp
 * S3ClientRequestSender class implementation
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

#include <aws/s3/S3Client.h>

namespace org::apache::nifi::minifi::aws::s3 {

std::optional<Aws::S3::Model::PutObjectResult> S3ClientRequestSender::sendPutObjectRequest(
    const Aws::S3::Model::PutObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
  auto outcome = s3_client.PutObject(request);

  if (outcome.IsSuccess()) {
      logger_->log_debug("Added S3 object '%s' to bucket '%s'", request.GetKey(), request.GetBucket());
      return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("PutS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

bool S3ClientRequestSender::sendDeleteObjectRequest(
    const Aws::S3::Model::DeleteObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, true);
  Aws::S3::Model::DeleteObjectOutcome outcome = s3_client.DeleteObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Deleted S3 object '%s' from bucket '%s'", request.GetKey(), request.GetBucket());
    return true;
  } else if (outcome.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY) {
    logger_->log_debug("S3 object '%s' was not found in bucket '%s'", request.GetKey(), request.GetBucket());
    return true;
  } else {
    logger_->log_error("DeleteS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return false;
  }
}

std::optional<Aws::S3::Model::GetObjectResult> S3ClientRequestSender::sendGetObjectRequest(
    const Aws::S3::Model::GetObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, true);
  auto outcome = s3_client.GetObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Fetched S3 object '%s' from bucket '%s'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("FetchS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::ListObjectsV2Result> S3ClientRequestSender::sendListObjectsRequest(
    const Aws::S3::Model::ListObjectsV2Request& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, true);
  auto outcome = s3_client.ListObjectsV2(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListObjectsV2 successful of bucket '%s'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectsV2 failed with the following: '%s'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::ListObjectVersionsResult> S3ClientRequestSender::sendListVersionsRequest(
    const Aws::S3::Model::ListObjectVersionsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, true);
  auto outcome = s3_client.ListObjectVersions(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListObjectVersions successful of bucket '%s'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectVersions failed with the following: '%s'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::GetObjectTaggingResult> S3ClientRequestSender::sendGetObjectTaggingRequest(
    const Aws::S3::Model::GetObjectTaggingRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, true);
  auto outcome = s3_client.GetObjectTagging(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Got tags for S3 object '%s' from bucket '%s'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("GetObjectTagging failed with the following: '%s'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::HeadObjectResult> S3ClientRequestSender::sendHeadObjectRequest(
    const Aws::S3::Model::HeadObjectRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, true);
  auto outcome = s3_client.HeadObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("HeadS3Object successful for key '%s' from bucket '%s'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("HeadS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::CreateMultipartUploadResult> S3ClientRequestSender::sendCreateMultipartUploadRequest(
    const Aws::S3::Model::CreateMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
  auto outcome = s3_client.CreateMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("CreateMultipartUpload successful for key '%s' and bucket '%s'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("CreateMultipartUpload failed for key '%s' and bucket '%s' with the following: '%s'", request.GetKey(), request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::UploadPartResult> S3ClientRequestSender::sendUploadPartRequest(
    const Aws::S3::Model::UploadPartRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
  auto outcome = s3_client.UploadPart(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("UploadPart successful for key '%s' from bucket '%s' with part number %d", request.GetKey(), request.GetBucket(), request.GetPartNumber());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("UploadPart failed for key '%s' from bucket '%s' with part number %d with the following: '%s'",
      request.GetKey(), request.GetBucket(), request.GetPartNumber(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::CompleteMultipartUploadResult> S3ClientRequestSender::sendCompleteMultipartUploadRequest(
    const Aws::S3::Model::CompleteMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
  auto outcome = s3_client.CompleteMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("CompleteMultipartUpload successful for key '%s' from bucket '%s'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("CompleteMultipartUpload failed for key '%s' from bucket '%s' with the following: '%s'", request.GetKey(), request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3::Model::ListMultipartUploadsResult> S3ClientRequestSender::sendListMultipartUploadsRequest(
    const Aws::S3::Model::ListMultipartUploadsRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
  auto outcome = s3_client.ListMultipartUploads(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListMultipartUploads successful for bucket '%s'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListMultipartUploads failed for bucket '%s' with the following: '%s'", request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

bool S3ClientRequestSender::sendAbortMultipartUploadRequest(
    const Aws::S3::Model::AbortMultipartUploadRequest& request,
    const Aws::Auth::AWSCredentials& credentials,
    const Aws::Client::ClientConfiguration& client_config,
    bool use_virtual_addressing) {
  Aws::S3::S3Client s3_client(credentials, client_config, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, use_virtual_addressing);
  auto outcome = s3_client.AbortMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("AbortMultipartUpload successful for bucket '%s', key '%s', upload id '%s'", request.GetBucket(), request.GetKey(), request.GetUploadId());
    return true;
  } else {
    logger_->log_error("AbortMultipartUpload failed for bucket '%s', key '%s', upload id '%s' with the following: '%s'",
      request.GetBucket(), request.GetKey(), request.GetUploadId(), outcome.GetError().GetMessage());
    return false;
  }
}

}  // namespace org::apache::nifi::minifi::aws::s3
