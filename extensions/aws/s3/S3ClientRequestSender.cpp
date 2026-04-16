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

#include <aws/s3-crt/S3CrtClient.h>
#include <mutex>

namespace org::apache::nifi::minifi::aws::s3 {

S3ClientRequestSender::S3ClientRequestSender(const Aws::Auth::AWSCredentials& credentials, const Aws::Client::ClientConfiguration& client_config, bool use_virtual_addressing)
    : s3_client_(credentials, [&]() {
          Aws::S3Crt::ClientConfiguration config(client_config);
          config.useVirtualAddressing = use_virtual_addressing;
          return config;
        }()) {
}

std::optional<Aws::S3Crt::Model::PutObjectResult> S3ClientRequestSender::sendPutObjectRequest(const Aws::S3Crt::Model::PutObjectRequest& request) {
  auto outcome = s3_client_.PutObject(request);

  if (outcome.IsSuccess()) {
      logger_->log_debug("Added S3 object '{}' to bucket '{}'", request.GetKey(), request.GetBucket());
      return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("PutS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

bool S3ClientRequestSender::sendDeleteObjectRequest(const Aws::S3Crt::Model::DeleteObjectRequest& request) {
  Aws::S3Crt::Model::DeleteObjectOutcome outcome = s3_client_.DeleteObject(request);

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

std::optional<Aws::S3Crt::Model::GetObjectResult> S3ClientRequestSender::sendGetObjectRequest(const Aws::S3Crt::Model::GetObjectRequest& request) {
  auto outcome = s3_client_.GetObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Fetched S3 object '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("FetchS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::ListObjectsV2Result> S3ClientRequestSender::sendListObjectsRequest(const Aws::S3Crt::Model::ListObjectsV2Request& request) {
  auto outcome = s3_client_.ListObjectsV2(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListObjectsV2 successful of bucket '{}'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectsV2 failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::ListObjectVersionsResult> S3ClientRequestSender::sendListVersionsRequest(const Aws::S3Crt::Model::ListObjectVersionsRequest& request) {
  auto outcome = s3_client_.ListObjectVersions(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListObjectVersions successful of bucket '{}'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectVersions failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::GetObjectTaggingResult> S3ClientRequestSender::sendGetObjectTaggingRequest(const Aws::S3Crt::Model::GetObjectTaggingRequest& request) {
  auto outcome = s3_client_.GetObjectTagging(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("Got tags for S3 object '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("GetObjectTagging failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::HeadObjectResult> S3ClientRequestSender::sendHeadObjectRequest(const Aws::S3Crt::Model::HeadObjectRequest& request) {
  auto outcome = s3_client_.HeadObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("HeadS3Object successful for key '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("HeadS3Object failed with the following: '{}'", outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::CreateMultipartUploadResult> S3ClientRequestSender::sendCreateMultipartUploadRequest(const Aws::S3Crt::Model::CreateMultipartUploadRequest& request) {
  auto outcome = s3_client_.CreateMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("CreateMultipartUpload successful for key '{}' and bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("CreateMultipartUpload failed for key '{}' and bucket '{}' with the following: '{}'", request.GetKey(), request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::UploadPartResult> S3ClientRequestSender::sendUploadPartRequest(const Aws::S3Crt::Model::UploadPartRequest& request) {
  auto outcome = s3_client_.UploadPart(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("UploadPart successful for key '{}' from bucket '{}' with part number {}", request.GetKey(), request.GetBucket(), request.GetPartNumber());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("UploadPart failed for key '{}' from bucket '{}' with part number {} with the following: '{}'",
      request.GetKey(), request.GetBucket(), request.GetPartNumber(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::CompleteMultipartUploadResult> S3ClientRequestSender::sendCompleteMultipartUploadRequest(const Aws::S3Crt::Model::CompleteMultipartUploadRequest& request) {
  auto outcome = s3_client_.CompleteMultipartUpload(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("CompleteMultipartUpload successful for key '{}' from bucket '{}'", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("CompleteMultipartUpload failed for key '{}' from bucket '{}' with the following: '{}'", request.GetKey(), request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

std::optional<Aws::S3Crt::Model::ListMultipartUploadsResult> S3ClientRequestSender::sendListMultipartUploadsRequest(const Aws::S3Crt::Model::ListMultipartUploadsRequest& request) {
  auto outcome = s3_client_.ListMultipartUploads(request);

  if (outcome.IsSuccess()) {
    logger_->log_debug("ListMultipartUploads successful for bucket '{}'", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListMultipartUploads failed for bucket '{}' with the following: '{}'", request.GetBucket(), outcome.GetError().GetMessage());
    return std::nullopt;
  }
}

bool S3ClientRequestSender::sendAbortMultipartUploadRequest(const Aws::S3Crt::Model::AbortMultipartUploadRequest& request) {
  auto outcome = s3_client_.AbortMultipartUpload(request);

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
