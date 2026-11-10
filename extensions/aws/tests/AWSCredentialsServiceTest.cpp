// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <cstdlib>
#include <memory>

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "controllerservices/AWSCredentialsService.h"
#include "unit/TestUtils.h"
#include "core/controller/ControllerServiceNode.h"

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

namespace {

void setEnvironmentCredentials(const std::string& key, const std::string& secret_key) {
  #ifdef WIN32
  _putenv_s("AWS_ACCESS_KEY_ID", key.c_str());
  _putenv_s("AWS_SECRET_ACCESS_KEY", secret_key.c_str());
  #else
  setenv("AWS_ACCESS_KEY_ID", key.c_str(), 1);
  setenv("AWS_SECRET_ACCESS_KEY", secret_key.c_str(), 1);
  #endif
}

TEST_CASE_METHOD(AWSCredentialsServiceTestAccessor, "Test expired credentials are refreshed", "[credentialRefresh]") {
  plan->setProperty(aws_credentials_service, minifi::aws::controllers::AWSCredentialsService::AccessKey, "key");
  plan->setProperty(aws_credentials_service, minifi::aws::controllers::AWSCredentialsService::SecretKey, "secret");
  aws_credentials_service->enable();
  assert(aws_credentials_service->getControllerServiceImplementation() != nullptr);
  auto aws_credentials_impl = aws_credentials_service->getControllerServiceImplementation<minifi::aws::controllers::AWSCredentialsService>();

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

TEST_CASE_METHOD(AWSCredentialsServiceTestAccessor, "Test credentials from default credential chain are always refreshed", "[credentialRefresh]") {
  setEnvironmentCredentials("key", "secret");
  plan->setProperty(aws_credentials_service, minifi::aws::controllers::AWSCredentialsService::UseDefaultCredentials, "true");
  aws_credentials_service->enable();
  assert(aws_credentials_service->getControllerServiceImplementation() != nullptr);
  auto aws_credentials_impl = aws_credentials_service->getControllerServiceImplementation<minifi::aws::controllers::AWSCredentialsService>();

  // Check intial credentials
  REQUIRE(aws_credentials_impl->getAWSCredentials());
  REQUIRE(aws_credentials_impl->getAWSCredentials()->GetAWSAccessKeyId() == "key");
  REQUIRE(aws_credentials_impl->getAWSCredentials()->GetAWSSecretKey() == "secret");
  REQUIRE_FALSE(aws_credentials_impl->getAWSCredentials()->IsExpired());

  // Set new credentials
  setEnvironmentCredentials("key2", "secret2");

  // Check for credential refresh
  REQUIRE(aws_credentials_impl->getAWSCredentials());
  REQUIRE(aws_credentials_impl->getAWSCredentials()->GetAWSAccessKeyId() == "key2");
  REQUIRE(aws_credentials_impl->getAWSCredentials()->GetAWSSecretKey() == "secret2");
  REQUIRE_FALSE(aws_credentials_impl->getAWSCredentials()->IsExpired());
}

}  // namespace
