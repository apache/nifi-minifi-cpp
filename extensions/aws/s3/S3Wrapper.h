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
#include <aws/s3/S3Client.h>
#include <aws/s3/model/Bucket.h>

#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

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

class S3Wrapper : public AbstractS3Wrapper {
public:
  S3Wrapper() {
  }

  void setCredentials(const Aws::Auth::AWSCredentials& cred) override {
    credentials_ = cred;
  }

  utils::optional<PutObjectResult> putObject(const Aws::String& bucketName,
    const Aws::String& objectName,
    const Aws::String& region) {
      return PutObjectResult{};
  }

private:
  Aws::Auth::AWSCredentials credentials_;
};

} /* namespace processors */
} /* namespace aws */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
