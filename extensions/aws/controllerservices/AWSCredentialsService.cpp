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

#include "AWSCredentialsService.h"

#include "core/Resource.h"

namespace org::apache::nifi::minifi::aws::controllers {

void AWSCredentialsService::initialize() {
  setSupportedProperties(Properties);
}

void AWSCredentialsService::onEnable() {
  std::string value;
  if (getProperty(AccessKey, value)) {
    aws_credentials_provider_.setAccessKey(value);
  }
  if (getProperty(SecretKey, value)) {
    aws_credentials_provider_.setSecretKey(value);
  }
  if (getProperty(CredentialsFile, value)) {
    aws_credentials_provider_.setCredentialsFile(value);
  }
  bool use_default_credentials = false;
  if (getProperty(UseDefaultCredentials, use_default_credentials)) {
    aws_credentials_provider_.setUseDefaultCredentials(use_default_credentials);
  }
}

std::optional<Aws::Auth::AWSCredentials> AWSCredentialsService::getAWSCredentials() {
  if (aws_credentials_provider_.getUseDefaultCredentials() || !aws_credentials_ || aws_credentials_->IsExpiredOrEmpty()) {
    aws_credentials_ = aws_credentials_provider_.getAWSCredentials();
  }
  return aws_credentials_;
}

REGISTER_RESOURCE(AWSCredentialsService, ControllerService);

}  // namespace org::apache::nifi::minifi::aws::controllers
