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
#include <utility>

#include "aws/core/auth/AWSCredentials.h"

#include "utils/AWSInitializer.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "AWSCredentialsProvider.h"

class AWSCredentialsServiceTestAccessor;

namespace org::apache::nifi::minifi::aws::controllers {

class AWSCredentialsService : public core::controller::ControllerService {
 public:
  explicit AWSCredentialsService(std::string name, const minifi::utils::Identifier &uuid = {})
      : ControllerService(std::move(name), uuid) {
  }

  explicit AWSCredentialsService(std::string name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerService(std::move(name)) {
  }

  EXTENSIONAPI static constexpr const char* Description = "AWS Credentials Management Service";

  EXTENSIONAPI static const core::Property UseDefaultCredentials;
  EXTENSIONAPI static const core::Property AccessKey;
  EXTENSIONAPI static const core::Property SecretKey;
  EXTENSIONAPI static const core::Property CredentialsFile;
  static auto properties() {
    return std::array{
      UseDefaultCredentials,
      AccessKey,
      SecretKey,
      CredentialsFile
    };
  }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

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

}  // namespace org::apache::nifi::minifi::aws::controllers
