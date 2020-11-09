/**
 * @file S3Wrapper.cpp
 * S3Wrapper class implementation
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
#include "S3Wrapper.h"

#include <aws/s3/S3Client.h>
#include <aws/s3/model/Bucket.h>
#include <aws/s3/model/StorageClass.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

minifi::utils::optional<Aws::S3::Model::PutObjectResult> S3Wrapper::sendPutObjectRequest(const Aws::S3::Model::PutObjectRequest& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  auto outcome = s3_client.PutObject(request);

  if (outcome.IsSuccess()) {
      logger_->log_info("Added S3 object '%s' to bucket '%s'", request.GetKey(), request.GetBucket());
      return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("PutS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return minifi::utils::nullopt;
  }
}

bool S3Wrapper::sendDeleteObjectRequest(const Aws::S3::Model::DeleteObjectRequest& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  Aws::S3::Model::DeleteObjectOutcome outcome = s3_client.DeleteObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_info("Deleted S3 object '%s' from bucket '%s'", request.GetKey(), request.GetBucket());
    return true;
  } else if (outcome.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY) {
    logger_->log_info("S3 object '%s' was not found in bucket '%s'", request.GetKey(), request.GetBucket());
    return true;
  } else {
    logger_->log_error("DeleteS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return false;
  }
}

minifi::utils::optional<Aws::S3::Model::GetObjectResult> S3Wrapper::sendGetObjectRequest(const Aws::S3::Model::GetObjectRequest& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  auto outcome = s3_client.GetObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_info("Fetched S3 object %s from bucket %s", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("FetchS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return minifi::utils::nullopt;
  }
}

minifi::utils::optional<Aws::S3::Model::ListObjectsV2Result> S3Wrapper::sendListObjectsRequest(const Aws::S3::Model::ListObjectsV2Request& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  auto outcome = s3_client.ListObjectsV2(request);

  if (outcome.IsSuccess()) {
    logger_->log_info("ListObjectsV2 successful of bucket %s", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectsV2 failed with the following: '%s'", outcome.GetError().GetMessage());
    return minifi::utils::nullopt;
  }
}

minifi::utils::optional<Aws::S3::Model::ListObjectVersionsResult> S3Wrapper::sendListVersionsRequest(const Aws::S3::Model::ListObjectVersionsRequest& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  auto outcome = s3_client.ListObjectVersions(request);

  if (outcome.IsSuccess()) {
    logger_->log_info("ListObjectVersions successful of bucket %s", request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("ListObjectVersions failed with the following: '%s'", outcome.GetError().GetMessage());
    return minifi::utils::nullopt;
  }
}

minifi::utils::optional<Aws::S3::Model::GetObjectTaggingResult> S3Wrapper::sendGetObjectTaggingRequest(const Aws::S3::Model::GetObjectTaggingRequest& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  auto outcome = s3_client.GetObjectTagging(request);

  if (outcome.IsSuccess()) {
    logger_->log_info("Got tags for S3 object %s from bucket %s", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("GetObjectTagging failed with the following: '%s'", outcome.GetError().GetMessage());
    return minifi::utils::nullopt;
  }
}

minifi::utils::optional<Aws::S3::Model::HeadObjectResult> S3Wrapper::sendHeadObjectRequest(const Aws::S3::Model::HeadObjectRequest& request) {
  Aws::S3::S3Client s3_client(credentials_, client_config_);
  auto outcome = s3_client.HeadObject(request);

  if (outcome.IsSuccess()) {
    logger_->log_info("HeadS3Object successful for key %s from bucket %s", request.GetKey(), request.GetBucket());
    return outcome.GetResultWithOwnership();
  } else {
    logger_->log_error("HeadS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
    return minifi::utils::nullopt;
  }
}

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
