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

minifi::utils::optional<PutObjectResult> S3Wrapper::putObject(const Aws::S3::Model::PutObjectRequest& request) {
  Aws::S3::S3Client s3_client(client_config_);
  Aws::S3::Model::PutObjectOutcome outcome = s3_client.PutObject(request);

  if (outcome.IsSuccess()) {
      logger_->log_info("Added S3 object %s to bucket %s", request.GetKey(), request.GetBucket());
      PutObjectResult result;
      result.version = outcome.GetResult().GetVersionId();
      result.etag = outcome.GetResult().GetETag();
      result.expiration = outcome.GetResult().GetExpiration();
      result.ssealgorithm = outcome.GetResult().GetSSECustomerAlgorithm();
      return result;
  } else {
      logger_->log_error("PutS3Object failed with the following: '%s'", outcome.GetError().GetMessage());
      return minifi::utils::nullopt;
  }
}

}  // namespace s3
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
