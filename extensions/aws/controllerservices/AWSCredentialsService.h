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

#ifndef NIFI_MINIFI_CPP_AWSCREDENTIALSCONTROLLERSERVICE_H
#define NIFI_MINIFI_CPP_AWSCREDENTIALSCONTROLLERSERVICE_H

#include <string>
#include <iostream>
#include <memory>
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/FlowConfiguration.h"

#include <aws/s3/S3Client.h>
#include <aws/s3/model/Bucket.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>

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

  explicit AWSCredentialsService(const std::string &name, const std::string &id)
      : ControllerService(name, id),
        logger_(logging::LoggerFactory<AWSCredentialsService>::getLogger()) {
  }

  explicit AWSCredentialsService(const std::string &name, const utils::Identifier& uuid = {})
      : ControllerService(name, uuid),
        logger_(logging::LoggerFactory<AWSCredentialsService>::getLogger()) {
  }

  explicit AWSCredentialsService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name),
        logger_(logging::LoggerFactory<AWSCredentialsService>::getLogger()) {

  }

  virtual void initialize() override;

  virtual void yield() override {

  };

  virtual bool isWorkAvailable() override {
    return false;
  };

  virtual bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  virtual void onEnable() override;

  Aws::Auth::AWSCredentials getAWSCredentials() {
    return awsCredentials;
  }

 private:
  std::string s3Ack, s3Secret;
  Aws::Auth::AWSCredentials awsCredentials;
  Aws::Client::ClientConfiguration client_config_;
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(AWSCredentialsService, "AWS Credentials Management Service");

} /* namespace controllers */
} /* namespace AWS */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // NIFI_MINIFI_CPP_AWSCREDENTIALSCONTROLLERSERVICE_H
