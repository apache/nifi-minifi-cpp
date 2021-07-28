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

#include <memory>
#include <optional>
#include <string>

#include "aws/core/auth/AWSCredentials.h"

#include "utils/AWSInitializer.h"
#include "core/Resource.h"
#include "utils/OptionalUtils.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "AWSCredentialsProvider.h"

class AWSCredentialsServiceTestAccessor;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace controllers {

class AWSCredentialsService : public core::controller::ControllerService {
 public:
  static const core::Property UseDefaultCredentials;
  static const core::Property AccessKey;
  static const core::Property SecretKey;
  static const core::Property CredentialsFile;

  explicit AWSCredentialsService(const std::string &name, const minifi::utils::Identifier &uuid = {})
      : ControllerService(name, uuid) {
  }

  explicit AWSCredentialsService(const std::string &name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerService(name) {
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

  std::optional<Aws::Auth::AWSCredentials> getAWSCredentials();

 private:
  friend class ::AWSCredentialsServiceTestAccessor;

  const utils::AWSInitializer& AWS_INITIALIZER = utils::AWSInitializer::get();
  std::optional<Aws::Auth::AWSCredentials> aws_credentials_;
  AWSCredentialsProvider aws_credentials_provider_;
};

}  // namespace controllers
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
