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

#include <stdlib.h>
#include <memory>

#include "../TestBase.h"
#include "controllerservices/AWSCredentialsService.h"
#include "../Utils.h"

class AWSCredentialsServiceTestAccessor {
 public:
  AWSCredentialsServiceTestAccessor() {
    // Disable retrieving AWS metadata for tests
    #ifdef WIN32
    _putenv_s("AWS_EC2_METADATA_DISABLED", "true");
    #else
    setenv("AWS_EC2_METADATA_DISABLED", "true", 1);
    #endif

    plan = test_controller.createPlan();
    aws_credentials_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  }

  FIELD_ACCESSOR(aws_credentials_);

 protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::controller::ControllerServiceNode> aws_credentials_service;
};

TEST_CASE_METHOD(AWSCredentialsServiceTestAccessor, "Test expired credentials are refreshed", "[credentialRefresh]") {
  plan->setProperty(aws_credentials_service, "Access Key", "key");
  plan->setProperty(aws_credentials_service, "Secret Key", "secret");
  aws_credentials_service->enable();
  assert(aws_credentials_service->getControllerServiceImplementation() != nullptr);
  auto aws_credentials_impl = std::static_pointer_cast<minifi::aws::controllers::AWSCredentialsService>(aws_credentials_service->getControllerServiceImplementation());

  // Check intial credentials
  REQUIRE(aws_credentials_impl->getAWSCredentials());
  REQUIRE(aws_credentials_impl->getAWSCredentials()->GetAWSAccessKeyId() == "key");
  REQUIRE(aws_credentials_impl->getAWSCredentials()->GetAWSSecretKey() == "secret");
  REQUIRE_FALSE(aws_credentials_impl->getAWSCredentials()->IsExpired());

  // Expire credentials
  get_aws_credentials_(*aws_credentials_impl)->SetExpiration(Aws::Utils::DateTime(0.0));
  REQUIRE(get_aws_credentials_(*aws_credentials_impl)->IsExpired());

  // Check for credential refresh
  REQUIRE_FALSE(aws_credentials_impl->getAWSCredentials()->IsExpired());
}
