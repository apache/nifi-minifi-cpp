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
  getProperty(AccessKey.getName(), access_key_);
  getProperty(SecretKey.getName(), secret_key_);
  getProperty(CredentialsFile.getName(), credentials_file_);
  getProperty(UseDefaultCredentials.getName(), use_default_credentials_);

  aws_credentials_provider_.setAccessKey(access_key_);
  aws_credentials_provider_.setSecretKey(secret_key_);
  aws_credentials_provider_.setCredentialsFile(credentials_file_);
  aws_credentials_provider_.setUseDefaultCredentials(use_default_credentials_);

  auto aws_credentials_result = aws_credentials_provider_.getAWSCredentials();
  if (aws_credentials_result) {
    aws_credentials_ = aws_credentials_result.value();
  }
}

}  // namespace controllers
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
