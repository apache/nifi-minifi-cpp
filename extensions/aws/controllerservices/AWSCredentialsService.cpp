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
#include "utils/expected.h"

namespace org::apache::nifi::minifi::aws::controllers {

void AWSCredentialsService::initialize() {
  setSupportedProperties(Properties);
}

void AWSCredentialsService::onEnable() {
  if (const auto access_key = getProperty(AccessKey.name)) {
    aws_credentials_provider_.setAccessKey(*access_key);
  }
  if (const auto secret_key = getProperty(SecretKey.name)) {
    aws_credentials_provider_.setSecretKey(*secret_key);
  }
  if (const auto credentials_file = getProperty(CredentialsFile.name)) {
    aws_credentials_provider_.setCredentialsFile(*credentials_file);
  }
  if (const auto use_credentials = getProperty(UseDefaultCredentials.name) | minifi::utils::andThen(parsing::parseBool)) {
    aws_credentials_provider_.setUseDefaultCredentials(*use_credentials);
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
