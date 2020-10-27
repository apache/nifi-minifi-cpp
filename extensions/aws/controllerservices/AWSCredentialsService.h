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

#pragma once

#include <string>
#include <iostream>
#include <memory>

#include "aws/s3/model/Bucket.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/core/Aws.h"
#include "aws/core/auth/AWSCredentials.h"
#include "aws/s3/S3Client.h"

#include "utils/AWSInitializer.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/FlowConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace controllers {

class AWSCredentialsService : public core::controller::ControllerService {
 public:
  static core::Property AccessKey;
  static core::Property SecretKey;

  explicit AWSCredentialsService(const std::string &name, const minifi::utils::Identifier& uuid = {})
      : ControllerService(name, uuid),
        logger_(logging::LoggerFactory<AWSCredentialsService>::getLogger()) {
  }

  explicit AWSCredentialsService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name),
        logger_(logging::LoggerFactory<AWSCredentialsService>::getLogger()) {
  }

  void initialize() override;

  void yield() override {
  };

  bool isWorkAvailable() override {
    return false;
  };

  bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  void onEnable() override;

  Aws::Auth::AWSCredentials getAWSCredentials() {
    return awsCredentials;
  }

 private:
  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::string s3Ack, s3Secret;
  Aws::Auth::AWSCredentials awsCredentials;
  Aws::Client::ClientConfiguration client_config_;
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(AWSCredentialsService, "AWS Credentials Management Service");

}  // namespace controllers
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
