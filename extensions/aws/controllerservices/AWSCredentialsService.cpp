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

core::Property AWSCredentialsService::AccessKey(
    core::PropertyBuilder::createProperty("Access Key")->withDescription("Specifies the AWS Access Key.")->isRequired(true)->supportsExpressionLanguage(true)->build());

core::Property AWSCredentialsService::SecretKey(
    core::PropertyBuilder::createProperty("Secret Key")->withDescription("Specifies the AWS Secret Key.")->isRequired(true)->supportsExpressionLanguage(true)->build());

void AWSCredentialsService::initialize() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(AccessKey);
  supportedProperties.insert(SecretKey);
  setSupportedProperties(supportedProperties);
}

void AWSCredentialsService::onEnable() {
  getProperty(AccessKey.getName(), s3Ack);
  getProperty(SecretKey.getName(), s3Secret);

  Aws::String awsS3Secret = s3Secret.c_str();
  Aws::String awsS3Ack = s3Ack.c_str();

  Aws::Auth::AWSCredentials creds(awsS3Ack, awsS3Secret);
  awsCredentials = creds;
}

}  // namespace controllers
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
