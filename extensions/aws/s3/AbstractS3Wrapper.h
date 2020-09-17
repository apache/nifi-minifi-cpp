/**
 * @file AbstractS3Client.h
 * AbstractS3Client class declaration
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

#include "utils/OptionalUtils.h"

#include <aws/s3/S3Client.h>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

struct PutObjectResult {
  Aws::String version;
  Aws::String etag;
  Aws::String expiration;
  Aws::String ssealgorithm;
};

class AbstractS3Wrapper {
public:
  virtual void setCredentials(const Aws::Auth::AWSCredentials& cred) = 0;
  virtual utils::optional<PutObjectResult> putObject(const Aws::String& bucketName,
    const Aws::String& objectName,
    const Aws::String& region) = 0;

  virtual ~AbstractS3Wrapper() = default;

// s3.bucket	The S3 bucket where the Object was put in S3
// s3.key	The S3 key within where the Object was put in S3
// s3.contenttype	The S3 content type of the S3 Object that put in S3
// s3.version	The version of the S3 Object that was put to S3
// s3.etag	The ETag of the S3 Object
// s3.uploadId	The uploadId used to upload the Object to S3 - multipart only
// s3.expiration	A human-readable form of the expiration date of the S3 object, if one is set
// s3.sseAlgorithm	The server side encryption algorithm of the object
// s3.usermetadata

// bucket - input
// key - input objectName
// content type - input from properties
// metadata - dynamic properties input

//PutObjectResult
// version - GetVersionId
// etag - GetETag
// expiration - GetExpiration
// ssealgorithm - GetSSECustomerAlgorithm

};

} /* namespace processors */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
