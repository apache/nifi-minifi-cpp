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

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/StorageClass.h>
#include <aws/s3/model/ServerSideEncryption.h>
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

static const std::map<std::string, Aws::S3::Model::StorageClass> storage_class_map {
  {"Standard", Aws::S3::Model::StorageClass::STANDARD},
  {"ReducedRedundancy", Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY}
};

static const std::map<std::string, Aws::S3::Model::ServerSideEncryption> server_side_encryption_map {
  {"None", Aws::S3::Model::ServerSideEncryption::NOT_SET},
  {"AES256", Aws::S3::Model::ServerSideEncryption::AES256},
  {"aws_kms", Aws::S3::Model::ServerSideEncryption::aws_kms},
};

struct PutS3ObjectOptions {
  std::string bucket_name;
  std::string object_key;
  std::string storage_class;
  std::string server_side_encryption;
};

class AbstractS3Wrapper {
public:
  void setCredentials(const Aws::Auth::AWSCredentials& cred) {
    credentials_ = cred;
  }

  void setRegion(const Aws::String& region) {
    client_config_.region = region;
  }

  void setTimeout(uint64_t timeout) {
    client_config_.connectTimeoutMs = timeout;
  }

  void setEndpointOverrideUrl(const Aws::String& url) {
    client_config_.endpointOverride = url;
  }

  utils::optional<PutObjectResult> putObject(const PutS3ObjectOptions& options, std::shared_ptr<Aws::IOStream> data_stream) {
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(options.bucket_name);
    request.SetKey(options.object_key);
    request.SetStorageClass(storage_class_map.at(options.storage_class));
    request.SetServerSideEncryption(server_side_encryption_map.at(options.server_side_encryption));
    request.SetBody(data_stream);

    return putObject(request);
  }

  virtual ~AbstractS3Wrapper() = default;

protected:
  virtual utils::optional<PutObjectResult> putObject(const Aws::S3::Model::PutObjectRequest& request) = 0;

  Aws::Client::ClientConfiguration client_config_;
  Aws::Auth::AWSCredentials credentials_;
};

} /* namespace processors */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
