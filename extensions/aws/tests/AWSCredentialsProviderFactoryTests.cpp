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

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "AWSCredentialsProviderFactory.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestUtils.h"

namespace org::apache::nifi::minifi::test {

using namespace std::literals::chrono_literals;

class AWSCredentialsProviderFactoryTestFixture {
 public:
  AWSCredentialsProviderFactoryTestFixture() {
    LogTestController::getInstance().setTrace<AWSCredentialsProviderFactoryTestFixture>();
  }

  AWSCredentialsProviderFactoryTestFixture(const AWSCredentialsProviderFactoryTestFixture&) = delete;
  AWSCredentialsProviderFactoryTestFixture(AWSCredentialsProviderFactoryTestFixture&&) = delete;
  AWSCredentialsProviderFactoryTestFixture& operator=(const AWSCredentialsProviderFactoryTestFixture&) = delete;
  AWSCredentialsProviderFactoryTestFixture& operator=(AWSCredentialsProviderFactoryTestFixture&&) = delete;

  ~AWSCredentialsProviderFactoryTestFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  std::shared_ptr<Aws::Auth::AWSCredentialsProvider> createProvider(const aws::AWSCredentialsProviderSettings& settings) {
    return createAWSCredentialsProvider(settings, *logger_);
  }

  TestController test_controller_;
  std::filesystem::path test_dir_{test_controller_.createTempDirectory()};
  utils::ScopedEnvironmentVariable access_key_env_{"AWS_ACCESS_KEY_ID", std::nullopt};
  utils::ScopedEnvironmentVariable secret_key_env_{"AWS_SECRET_ACCESS_KEY", std::nullopt};
  utils::ScopedEnvironmentVariable session_token_env_{"AWS_SESSION_TOKEN", std::nullopt};
  utils::ScopedEnvironmentVariable profile_env_{"AWS_PROFILE", std::nullopt};
  utils::ScopedEnvironmentVariable role_arn_env_{"AWS_ROLE_ARN", std::nullopt};
  utils::ScopedEnvironmentVariable web_identity_token_file_env_{"AWS_WEB_IDENTITY_TOKEN_FILE", std::nullopt};
  utils::ScopedEnvironmentVariable shared_credentials_file_env_{"AWS_SHARED_CREDENTIALS_FILE", (test_dir_ / "credentials").string()};
  utils::ScopedEnvironmentVariable config_file_env_{"AWS_CONFIG_FILE", (test_dir_ / "config").string()};
  std::shared_ptr<logging::Logger> logger_ = core::logging::LoggerFactory<AWSCredentialsProviderFactoryTestFixture>::getLogger();
};

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Without a strategy the access key and secret key are used",
    "[awsCredentialsProviderFactory]") {
  const auto provider = createProvider({.access_key = "key", .secret_key = "secret"});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Without a strategy the credentials file is used when no keys are set",
    "[awsCredentialsProviderFactory]") {
  const auto credentials_file = utils::putFileToDir(test_dir_, "aws_credentials.conf", "accessKey=file_key\nsecretKey=file_secret\n");
  const auto provider = createProvider({.credentials_file = credentials_file.string()});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "file_key");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "file_secret");
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Without a strategy and without credentials no provider is created",
    "[awsCredentialsProviderFactory]") {
  CHECK(createProvider({}) == nullptr);
  CHECK(LogTestController::getInstance().contains("No Credential Configuration Strategy was set and no AWS credentials were set."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "From Properties strategy uses the access key and secret key",
    "[awsCredentialsProviderFactory]") {
  const auto provider = createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::FromProperties,
      .access_key = "key",
      .secret_key = "secret"});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "key");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "From Properties strategy does not fall back to other credential sources",
    "[awsCredentialsProviderFactory]") {
  const auto credentials_file = utils::putFileToDir(test_dir_, "aws_credentials.conf", "accessKey=file_key\nsecretKey=file_secret\n");
  aws::AWSCredentialsProviderSettings settings{.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::FromProperties,
      .credentials_file = credentials_file.string()};

  SECTION("Neither key is set") {
  }
  SECTION("Only the access key is set") {
    settings.access_key = "key";
  }
  SECTION("Only the secret key is set") {
    settings.secret_key = "secret";
  }

  CHECK(createProvider(settings) == nullptr);
  CHECK(LogTestController::getInstance().contains("Access key or secret key is missing for FromProperties strategy."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Credentials File strategy reads the keys from the credentials file",
    "[awsCredentialsProviderFactory]") {
  const auto credentials_file = utils::putFileToDir(test_dir_, "aws_credentials.conf", "accessKey=file_key\nsecretKey=file_secret\n");
  const auto provider = createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::CredentialsFile,
      .credentials_file = credentials_file.string()});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "file_key");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "file_secret");
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Credentials File strategy does not create a missing credentials file",
    "[awsCredentialsProviderFactory]") {
  const auto missing_directory = test_controller_.createTempDirectory() / "missing";
  const auto credentials_file = missing_directory / "aws_credentials.conf";

  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::CredentialsFile,
            .credentials_file = credentials_file.string()}) == nullptr);
  CHECK(LogTestController::getInstance().contains("The specified credentials file does not exist!"));
  CHECK(!std::filesystem::exists(missing_directory));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Credentials File strategy fails when the credentials file has no keys",
    "[awsCredentialsProviderFactory]") {
  std::string content;
  SECTION("The file is empty") {
    content = "";
  }
  SECTION("The file has unrelated properties only") {
    content = "someProperty=someValue\n";
  }
  SECTION("The keys are empty") {
    content = "accessKey=\nsecretKey=\n";
  }
  SECTION("The secret key is missing") {
    content = "accessKey=file_key\n";
  }

  const auto credentials_file = utils::putFileToDir(test_dir_, "aws_credentials.conf", content);
  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::CredentialsFile,
            .credentials_file = credentials_file.string()}) == nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in the credentials file."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Credentials File strategy fails when the credentials file path is empty",
    "[awsCredentialsProviderFactory]") {
  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::CredentialsFile}) == nullptr);
  CHECK(LogTestController::getInstance().contains("Credentials file path is empty for CredentialsFile strategy."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Environment Variables strategy reads the credentials from the environment",
    "[awsCredentialsProviderFactory]") {
  const utils::ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "env_key"};
  const utils::ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret"};

  const auto provider = createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::EnvironmentVariables});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "env_key");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "env_secret");

  const utils::ScopedEnvironmentVariable new_access_key{"AWS_ACCESS_KEY_ID", "env_key2"};
  const utils::ScopedEnvironmentVariable new_secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret2"};
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "env_key2");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "env_secret2");
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Environment Variables strategy fails when the environment variables are not set",
    "[awsCredentialsProviderFactory]") {
  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::EnvironmentVariables}) == nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in environment variables."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Default Credentials strategy resolves the credentials from the environment",
    "[awsCredentialsProviderFactory]") {
  const utils::ScopedEnvironmentVariable access_key{"AWS_ACCESS_KEY_ID", "env_key"};
  const utils::ScopedEnvironmentVariable secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret"};

  const auto provider = createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::DefaultCredentials});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "env_key");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "env_secret");

  const utils::ScopedEnvironmentVariable new_access_key{"AWS_ACCESS_KEY_ID", "env_key2"};
  const utils::ScopedEnvironmentVariable new_secret_key{"AWS_SECRET_ACCESS_KEY", "env_secret2"};
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == "env_key2");
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == "env_secret2");
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Default Credentials strategy fails when no credentials are available",
    "[awsCredentialsProviderFactory]") {
  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::DefaultCredentials}) == nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in the default AWS credentials provider chain."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Profile Credentials strategy reads the credentials from the shared credentials file",
    "[awsCredentialsProviderFactory]") {
  const auto profile_file = utils::putFileToDir(test_dir_,
      "credentials",
      "[default]\n"
      "aws_access_key_id = default_key\n"
      "aws_secret_access_key = default_secret\n"
      "\n"
      "[test_profile]\n"
      "aws_access_key_id = test_profile_key\n"
      "aws_secret_access_key = test_profile_secret\n");
  const utils::ScopedEnvironmentVariable shared_credentials_file{"AWS_SHARED_CREDENTIALS_FILE", profile_file.string()};

  std::string profile_name;
  std::string expected_access_key;
  std::string expected_secret_key;
  SECTION("The default profile is used when no profile name is set") {
    expected_access_key = "default_key";
    expected_secret_key = "default_secret";
  }
  SECTION("The configured profile is used when a profile name is set") {
    profile_name = "test_profile";
    expected_access_key = "test_profile_key";
    expected_secret_key = "test_profile_secret";
  }

  const auto provider = createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::ProfileCredentials,
      .profile_name = profile_name});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().GetAWSAccessKeyId() == expected_access_key);
  CHECK(provider->GetAWSCredentials().GetAWSSecretKey() == expected_secret_key);
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Profile Credentials strategy fails when the profile cannot be found",
    "[awsCredentialsProviderFactory]") {
  aws::AWSCredentialsProviderSettings settings{.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::ProfileCredentials};
  std::optional<utils::ScopedEnvironmentVariable> shared_credentials_file;

  SECTION("The shared credentials file does not exist") {
  }
  SECTION("The profile is not present in the shared credentials file") {
    const auto profile_file = utils::putFileToDir(test_dir_,
        "credentials",
        "[default]\naws_access_key_id = default_key\naws_secret_access_key = default_secret\n");
    shared_credentials_file.emplace("AWS_SHARED_CREDENTIALS_FILE", profile_file.string());
    settings.profile_name = "missing_profile";
  }

  CHECK(createProvider(settings) == nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in profile."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "SSO Credentials strategy fails without an SSO configuration",
    "[awsCredentialsProviderFactory]") {
  aws::AWSCredentialsProviderSettings settings{.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::SSOCredentials};
  SECTION("No SSO profile name is set") {
  }
  SECTION("An SSO profile name is set") {
    settings.sso_profile_name = "sso_profile";
  }

  CHECK(createProvider(settings) == nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in SSO."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "STS Web Identity strategy fails without a web identity token",
    "[awsCredentialsProviderFactory]") {
  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::STSAssumeRoleWebIdentityCredentials}) ==
      nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in STS Assume Role Web Identity."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Instance Profile strategy fails when the instance metadata service is unavailable",
    "[awsCredentialsProviderFactory]") {
  CHECK(createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::InstanceProfileCredentials}) == nullptr);
  CHECK(LogTestController::getInstance().contains("No AWS credentials were found in instance profile."));
}

TEST_CASE_METHOD(AWSCredentialsProviderFactoryTestFixture, "Anonymous Credentials strategy creates a provider with empty credentials",
    "[awsCredentialsProviderFactory]") {
  const auto provider = createProvider({.credential_configuration_strategy = aws::CredentialConfigurationStrategyOption::AnonymousCredentials});
  REQUIRE(provider != nullptr);
  CHECK(provider->GetAWSCredentials().IsEmpty());
}

}  // namespace org::apache::nifi::minifi::test
