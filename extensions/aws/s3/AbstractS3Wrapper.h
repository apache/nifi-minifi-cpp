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

struct PutS3ObjectOptions {
  Aws::String bucket_name;
  Aws::String object_key;
  Aws::S3::Model::StorageClass storage_class;
};

class AbstractS3Wrapper {
public:
  virtual void setCredentials(const Aws::Auth::AWSCredentials& cred) = 0;
  virtual void setRegion(const Aws::String& region) = 0;
  virtual void setTimeout(uint64_t timeout) = 0;
  virtual void setEndpointOverrideUrl(const Aws::String& url) = 0;
  virtual utils::optional<PutObjectResult> putObject(const PutS3ObjectOptions& options, std::shared_ptr<Aws::IOStream> data_stream) = 0;
  virtual ~AbstractS3Wrapper() = default;
};

} /* namespace processors */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
