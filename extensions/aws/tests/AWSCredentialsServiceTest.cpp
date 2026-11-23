// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "unit/TestBase.h"
#include "unit/TestUtils.h"
#include "unit/Catch.h"
#include "catch2/generators/catch_generators.hpp"
#include "controllerservices/AWSCredentialsService.h"
#include "core/controller/ControllerServiceNode.h"
#include "core/controller/StandardControllerServiceNode.h"

namespace {

using AWSCredentialsService = minifi::aws::controllers::AWSCredentialsService;
using minifi::test::utils::ScopedEnvironmentVariable;

class AWSCredentialsServiceTestFixture {
 public:
  AWSCredentialsServiceTestFixture() {
    LogTestController::getInstance().setWarn<minifi::core::controller::StandardControllerServiceNode>();
    plan = test_controller.createPlan();
    aws_credentials_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  }

  AWSCredentialsServiceTestFixture(const AWSCredentialsServiceTestFixture&) = delete;
  AWSCredentialsServiceTestFixture(AWSCredentialsServiceTestFixture&&) = delete;
  AWSCredentialsServiceTestFixture& operator=(const AWSCredentialsServiceTestFixture&) = delete;
  AWSCredentialsServiceTestFixture& operator=(AWSCredentialsServiceTestFixture&&) = delete;

  ~AWSCredentialsServiceTestFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  std::shared_ptr<AWSCredentialsService> getCredentialsServiceImplementation() {
    auto implementation = aws_credentials_service->getControllerServiceImplementation<AWSCredentialsService>();
    REQUIRE(implementation != nullptr);
    return implementation;
  }

  static bool logContains(const std::string& message) {
    return LogTestController::getInstance().contains(message, std::chrono::milliseconds{0});
  }

  TestController test_controller;
  // The default credential chain reads these, so the tests have to be isolated from the environment of the host.
  ScopedEnvironmentVariable access_key_env{"AWS_ACCESS_KEY_ID", std::nullopt};
  ScopedEnvironmentVariable secret_key_env{"AWS_SECRET_ACCESS_KEY", std::nullopt};
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::controller::ControllerServiceNode> aws_credentials_service;
};

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test credentials provider created from properties", "[credentialsProvider]") {
  plan->setProperty(aws_credentials_service, AWSCredentialsService::AccessKey, "key");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::SecretKey, "secret");
  REQUIRE(aws_credentials_service->enable());
  const auto aws_credentials_impl = getCredentialsServiceImplementation();

  auto credentials_provider = aws_credentials_impl->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "secret");

  // The same provider is handed out on every call, and it keeps resolving the configured credentials
  CHECK(aws_credentials_impl->getAWSCredentialsProvider() == credentials_provider);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test credentials from default credential chain are always refreshed", "[credentialsProvider]") {
  const ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "key"};
  const ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "secret"};
  plan->setProperty(aws_credentials_service, AWSCredentialsService::UseDefaultCredentials, "true");
  REQUIRE(aws_credentials_service->enable());

  auto credentials_provider = getCredentialsServiceImplementation()->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "secret");

  // Set new credentials
  const ScopedEnvironmentVariable new_access_key{"AWS_ACCESS_KEY_ID", "key2"};
  const ScopedEnvironmentVariable new_secret_key{"AWS_SECRET_ACCESS_KEY", "secret2"};

  // The provider picks up the new credentials without being rebuilt
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key2");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "secret2");
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test a failed credentials provider creation is not cached", "[credentialsProvider]") {
  plan->setProperty(aws_credentials_service, AWSCredentialsService::CredentialConfigurationStrategy, "Environment Variables");
  REQUIRE(aws_credentials_service->enable());
  const auto aws_credentials_impl = getCredentialsServiceImplementation();
  REQUIRE(aws_credentials_impl->getAWSCredentialsProvider() == nullptr);

  const ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "env_key"};
  const ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret"};

  auto credentials_provider = aws_credentials_impl->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "env_key");
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test credential settings are not carried over to the next enable", "[credentialsProvider]") {
  const ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "envkey"};
  const ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "envsecret"};
  plan->setProperty(aws_credentials_service, AWSCredentialsService::UseDefaultCredentials, "true");
  REQUIRE(aws_credentials_service->enable());
  const auto aws_credentials_impl = getCredentialsServiceImplementation();
  REQUIRE(aws_credentials_impl->getAWSCredentialsProvider()->GetAWSCredentials().GetAWSAccessKeyId() == "envkey");

  aws_credentials_service->disable();
  plan->setProperty(aws_credentials_service, AWSCredentialsService::UseDefaultCredentials, "false");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::AccessKey, "key");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::SecretKey, "secret");
  REQUIRE(aws_credentials_service->enable());

  auto credentials_provider = aws_credentials_impl->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test Credential Configuration Strategy values are case insensitive", "[credentialsProvider]") {
  const auto strategy = GENERATE(as<std::string>{}, "From Properties", "from properties", "FROM PROPERTIES", "fRoM pRoPeRtIeS");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::CredentialConfigurationStrategy, strategy);
  plan->setProperty(aws_credentials_service, AWSCredentialsService::AccessKey, "key");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::SecretKey, "secret");
  REQUIRE(aws_credentials_service->enable());

  auto credentials_provider = getCredentialsServiceImplementation()->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test the service cannot be enabled when both Use Default Credentials and Credential Configuration Strategy are set", "[credentialsProvider]") {
  const auto strategy = GENERATE(as<std::string>{}, "From Properties", "Environment Variables", "Anonymous Credentials");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::UseDefaultCredentials, "true");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::CredentialConfigurationStrategy, strategy);
  CHECK(!aws_credentials_service->enable());
  CHECK(logContains("Both Credential Configuration Strategy and Use Default Credentials properties are set!"));
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test Use Default Credentials can be set together with the Default Credentials strategy", "[credentialsProvider]") {
  const ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "env_key"};
  const ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret"};
  plan->setProperty(aws_credentials_service, AWSCredentialsService::UseDefaultCredentials, "true");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::CredentialConfigurationStrategy, "Default Credentials");
  REQUIRE(aws_credentials_service->enable());

  auto credentials_provider = getCredentialsServiceImplementation()->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "env_key");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "env_secret");
}

TEST_CASE_METHOD(AWSCredentialsServiceTestFixture, "Test the default value of Use Default Credentials is not treated as a strategy", "[credentialsProvider]") {
  const ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "env_key"};
  const ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret"};
  plan->setProperty(aws_credentials_service, AWSCredentialsService::CredentialConfigurationStrategy, "From Properties");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::AccessKey, "key");
  plan->setProperty(aws_credentials_service, AWSCredentialsService::SecretKey, "secret");
  REQUIRE(aws_credentials_service->enable());

  auto credentials_provider = getCredentialsServiceImplementation()->getAWSCredentialsProvider();
  REQUIRE(credentials_provider != nullptr);
  CHECK(credentials_provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(credentials_provider->GetAWSCredentials().GetAWSSecretKey() == "secret");
}

}  // namespace
