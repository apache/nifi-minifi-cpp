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

#include <set>

#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace controllers {

const core::Property AWSCredentialsService::UseDefaultCredentials(
    core::PropertyBuilder::createProperty("Use Default Credentials")
    ->withDescription("If true, uses the Default Credential chain, including EC2 instance profiles or roles, environment variables, default user credentials, etc.")
    ->withDefaultValue<bool>(false)
    ->isRequired(true)
    ->build());

const core::Property AWSCredentialsService::AccessKey(
    core::PropertyBuilder::createProperty("Access Key")->withDescription("Specifies the AWS Access Key.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property AWSCredentialsService::SecretKey(
    core::PropertyBuilder::createProperty("Secret Key")
    ->withDescription("Specifies the AWS Secret Key.")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Property AWSCredentialsService::CredentialsFile(
  core::PropertyBuilder::createProperty("Credentials File")
    ->withDescription("Path to a file containing AWS access key and secret key in properties file format. Properties used: accessKey and secretKey")
    ->build());

void AWSCredentialsService::initialize() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(AccessKey);
  supportedProperties.insert(SecretKey);
  supportedProperties.insert(UseDefaultCredentials);
  supportedProperties.insert(CredentialsFile);
  setSupportedProperties(supportedProperties);
}

void AWSCredentialsService::onEnable() {
  std::string value;
  if (getProperty(AccessKey.getName(), value)) {
    aws_credentials_provider_.setAccessKey(value);
  }
  if (getProperty(SecretKey.getName(), value)) {
    aws_credentials_provider_.setSecretKey(value);
  }
  if (getProperty(CredentialsFile.getName(), value)) {
    aws_credentials_provider_.setCredentialsFile(value);
  }
  bool use_default_credentials = false;
  if (getProperty(UseDefaultCredentials.getName(), use_default_credentials)) {
    aws_credentials_provider_.setUseDefaultCredentials(use_default_credentials);
  }
}

std::optional<Aws::Auth::AWSCredentials> AWSCredentialsService::getAWSCredentials() {
  if (aws_credentials_provider_.getUseDefaultCredentials() || !aws_credentials_ || aws_credentials_->IsExpiredOrEmpty()) {
    aws_credentials_ = aws_credentials_provider_.getAWSCredentials();
  }
  return aws_credentials_;
}

REGISTER_RESOURCE(AWSCredentialsService, "AWS Credentials Management Service");

}  // namespace controllers
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
