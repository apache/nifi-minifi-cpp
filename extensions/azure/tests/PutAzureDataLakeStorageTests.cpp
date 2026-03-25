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

#include "AzureDataLakeStorageTestsFixture.h"
#include "processors/PutAzureDataLakeStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace {

using namespace std::literals::chrono_literals;

using PutAzureDataLakeStorageTestsFixture = AzureDataLakeStorageTestsFixture<minifi::azure::processors::PutAzureDataLakeStorage>;

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::AzureStorageCredentialsService, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Test Azure credentials with account name and SAS token set", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken, "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  CHECK(passed_params.credentials.buildConnectionString() == "AccountName=TEST_ACCOUNT;SharedAccessSignature=token");
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Test Azure credentials with connection string override", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, CONNECTION_STRING);
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken, "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Test Azure credentials with Azure default identity sources", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  minifi::azure::CredentialConfigurationStrategyOption expected_configuration_strategy_option{};
  std::string credential_configuration_strategy_string;
  std::string managed_identity_client_id;
  SECTION("Managed Identity") {
    expected_configuration_strategy_option = minifi::azure::CredentialConfigurationStrategyOption::ManagedIdentity;
    credential_configuration_strategy_string = "Managed Identity";
    managed_identity_client_id = "test-managed-identity-client-id";
  }
  SECTION("Default Credential") {
    expected_configuration_strategy_option = minifi::azure::CredentialConfigurationStrategyOption::DefaultCredential;
    credential_configuration_strategy_string = "Default Credential";
  }
  SECTION("Workload Identity") {
    expected_configuration_strategy_option = minifi::azure::CredentialConfigurationStrategyOption::WorkloadIdentity;
    credential_configuration_strategy_string = "Workload Identity";
  }
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "test");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::CredentialConfigurationStrategy, credential_configuration_strategy_string);
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ManagedIdentityClientId, managed_identity_client_id);
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  CHECK(passed_params.credentials.buildConnectionString().empty());
  CHECK(passed_params.credentials.getStorageAccountName() == "TEST_ACCOUNT");
  CHECK(passed_params.credentials.getEndpointSuffix() == "core.windows.net");
  CHECK(passed_params.credentials.getCredentialConfigurationStrategy() == expected_configuration_strategy_option);
  CHECK(passed_params.credentials.getManagedIdentityClientId() == managed_identity_client_id);
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Both SAS Token and Storage Account Key cannot be set in credentials service") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken, "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountKey, "TEST_KEY");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setDynamicProperty(update_attribute_, "test.filesystemname", "");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  CHECK(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Upload to Azure Data Lake Storage with default parameters", "[azureDataLakeStorageUpload]") {
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.filename == GETFILE_FILE_NAME);
  CHECK_FALSE(passed_params.replace_file);
  REQUIRE(getFailedFlowFileContents().empty());
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:" + GETFILE_FILE_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:" + std::to_string(TEST_DATA.size())));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.primaryUri value:" + mock_data_lake_storage_client_ptr_->PRIMARY_URI + "\n"));
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "File creation fails", "[azureDataLakeStorageUpload]") {
  mock_data_lake_storage_client_ptr_->setFileCreationError(true);
  test_controller_.runSession(plan_, true);
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "File upload fails", "[azureDataLakeStorageUpload]") {
  mock_data_lake_storage_client_ptr_->setUploadFailure(true);
  test_controller_.runSession(plan_, true);
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Transfer to failure on 'fail' resolution strategy if file exists", "[azureDataLakeStorageUpload]") {
  mock_data_lake_storage_client_ptr_->setFileCreation(false);
  test_controller_.runSession(plan_, true);
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Transfer to success on 'ignore' resolution strategy if file exists", "[azureDataLakeStorageUpload]") {
  plan_->setProperty(azure_data_lake_storage_,
    minifi::azure::processors::PutAzureDataLakeStorage::ConflictResolutionStrategy, magic_enum::enum_name(minifi::azure::FileExistsResolutionStrategy::ignore));
  mock_data_lake_storage_client_ptr_->setFileCreation(false);
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().empty());
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:filename value:" + GETFILE_FILE_NAME));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure", 0s, 0ms));
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Replace old file on 'replace' resolution strategy if file exists", "[azureDataLakeStorageUpload]") {
  plan_->setProperty(azure_data_lake_storage_,
    minifi::azure::processors::PutAzureDataLakeStorage::ConflictResolutionStrategy, magic_enum::enum_name(minifi::azure::FileExistsResolutionStrategy::replace));
  mock_data_lake_storage_client_ptr_->setFileCreation(false);
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.filename == GETFILE_FILE_NAME);
  CHECK(passed_params.replace_file);
  REQUIRE(getFailedFlowFileContents().empty());
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:" + GETFILE_FILE_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:" + std::to_string(TEST_DATA.size())));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.primaryUri value:" + mock_data_lake_storage_client_ptr_->PRIMARY_URI + "\n"));
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Upload to Azure Data Lake Storage with empty directory is accepted", "[azureDataLakeStorageUpload]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::DirectoryName, "");
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  CHECK(passed_params.directory_name.empty());
  REQUIRE(getFailedFlowFileContents().empty());
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:\n"));
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Test Azure data lake storage upload using proxy", "[azureDataLakeStorageUpload]") {
  auto proxy_configuration_service = plan_->addController("ProxyConfigurationService", "ProxyConfigurationService");
  plan_->setProperty(proxy_configuration_service, "Proxy Server Host", "host");
  plan_->setProperty(proxy_configuration_service, "Proxy Server Port", "1234");
  plan_->setProperty(proxy_configuration_service, "Proxy User Name", "username");
  plan_->setProperty(proxy_configuration_service, "Proxy User Password", "password");
  plan_->setProperty(proxy_configuration_service, "Proxy Type", "HTTP");
  plan_->setProperty(azure_data_lake_storage_, "Proxy Configuration Service", "ProxyConfigurationService");

  test_controller_.runSession(plan_, true);

  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedPutParams();
  REQUIRE(passed_params.proxy_configuration);
  REQUIRE(passed_params.proxy_configuration->proxy_host == "host");
  REQUIRE(passed_params.proxy_configuration->proxy_port);
  REQUIRE(*passed_params.proxy_configuration->proxy_port == 1234);
  REQUIRE(passed_params.proxy_configuration->proxy_user);
  REQUIRE(*passed_params.proxy_configuration->proxy_user == "username");
  REQUIRE(passed_params.proxy_configuration->proxy_password);
  REQUIRE(*passed_params.proxy_configuration->proxy_password == "password");
  REQUIRE(passed_params.proxy_configuration->proxy_type == minifi::controllers::ProxyType::HTTP);
  CHECK(getFailedFlowFileContents().empty());
}

}  // namespace
