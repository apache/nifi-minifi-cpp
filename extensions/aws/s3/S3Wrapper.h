/**
 * @file S3.h
 * GetGPS class declaration
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

#include "AbstractS3Wrapper.h"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/AWSLogging.h>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/Bucket.h>
#include <aws/s3/model/StorageClass.h>
#include <aws/s3/model/PutObjectRequest.h>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

class S3Initializer{
 public:
  S3Initializer(){
    Aws::InitAPI(options);
    // Aws::Utils::Logging::InitializeAWSLogging(
    //     Aws::MakeShared<Aws::Utils::Logging::DefaultLogSystem>(
    //         "RunUnitTests", Aws::Utils::Logging::LogLevel::Trace, "aws_sdk_"));
  }

  ~S3Initializer(){
    // Aws::Utils::Logging::ShutdownAWSLogging();
    Aws::ShutdownAPI(options);
  }

 private:
  Aws::SDKOptions options;
};

static const std::map<std::string, Aws::S3::Model::StorageClass> storage_class_map {
  {"Standard", Aws::S3::Model::StorageClass::STANDARD},
  {"ReducedRedundancy", Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY}
};

class S3Wrapper : public AbstractS3Wrapper {
public:
  S3Wrapper() {
  }

  void setCredentials(const Aws::Auth::AWSCredentials& cred) override {
    credentials_ = cred;
  }

  void setRegion(const Aws::String& region) override {
    client_config_.region = region;
  }

  void setTimeout(uint64_t timeout) override {
    client_config_.connectTimeoutMs = timeout;
  }

  void setEndpointOverrideUrl(const Aws::String& url) override {
    client_config_.endpointOverride = url;
  }

  utils::optional<PutObjectResult> putObject(const PutS3ObjectOptions& options, std::shared_ptr<Aws::IOStream> data_stream) override {
    Aws::S3::S3Client s3_client(client_config_);

    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(options.bucket_name);
    request.SetKey(options.object_key);
    request.SetBody(data_stream);

    Aws::S3::Model::PutObjectOutcome outcome = s3_client.PutObject(request);

    if (outcome.IsSuccess()) {
        // std::cout << "Added object '" << objectName << "' to bucket '"
        //     << bucketName << "'.";
        PutObjectResult result;
        result.version = outcome.GetResult().GetVersionId();
        result.etag = outcome.GetResult().GetETag();
        result.expiration = outcome.GetResult().GetExpiration();
        result.ssealgorithm = outcome.GetResult().GetSSECustomerAlgorithm();
        return result;
    }
    else
    {
        // std::cout << "Error: PutObject: " <<
        //     outcome.GetError().GetMessage() << std::endl;
        return utils::nullopt;
    }
  }

private:
  Aws::Client::ClientConfiguration client_config_;
  Aws::Auth::AWSCredentials credentials_;
};

} /* namespace processors */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
