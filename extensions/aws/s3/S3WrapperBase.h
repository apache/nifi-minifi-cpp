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

#include <string>
#include <map>
#include <memory>

#include "aws/core/auth/AWSCredentialsProvider.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/StorageClass.h"
#include "aws/s3/model/ServerSideEncryption.h"

#include "utils/AWSInitializer.h"
#include "utils/OptionalUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace s3 {

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

struct PutS3RequestParameters {
  std::string bucket;
  std::string object_key;
  std::string storage_class;
  std::string server_side_encryption;
  std::string content_type;
  std::map<std::string, std::string> user_metadata_map;
  std::string fullcontrol_user_list;
  std::string read_permission_user_list;
  std::string read_acl_user_list;
  std::string write_acl_user_list;
};

struct ProxyOptions {
  std::string host;
  uint32_t port = 0;
  std::string username;
  std::string password;
};

class S3WrapperBase {
 public:
  void setCredentials(const Aws::Auth::AWSCredentials& cred);
  void setRegion(const Aws::String& region);
  void setTimeout(uint64_t timeout);
  void setEndpointOverrideUrl(const Aws::String& url);
  void setProxy(const ProxyOptions& proxy);

  minifi::utils::optional<PutObjectResult> putObject(const PutS3RequestParameters& options, std::shared_ptr<Aws::IOStream> data_stream);

  virtual ~S3WrapperBase() = default;

 protected:
  virtual minifi::utils::optional<PutObjectResult> putObject(const Aws::S3::Model::PutObjectRequest& request) = 0;

  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  Aws::Client::ClientConfiguration client_config_;
  Aws::Auth::AWSCredentials credentials_;
};

} /* namespace s3 */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
