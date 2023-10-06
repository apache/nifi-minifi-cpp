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
#include "processors/DeleteAzureDataLakeStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace {

using namespace std::literals::chrono_literals;

using DeleteAzureDataLakeStorageTestsFixture = AzureDataLakeStorageTestsFixture<minifi::azure::processors::DeleteAzureDataLakeStorage>;

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::DeleteAzureDataLakeStorage::AzureStorageCredentialsService, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Test Azure credentials with account name and SAS token set", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken, "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedDeleteParams();
  CHECK(passed_params.credentials.buildConnectionString() == "AccountName=TEST_ACCOUNT;SharedAccessSignature=token");
  CHECK(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Test Azure credentials with connection string override", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, CONNECTION_STRING);
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken, "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedDeleteParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Test Azure credentials with managed identity use", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "test");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::UseManagedIdentityCredentials, "true");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedDeleteParams();
  CHECK(passed_params.credentials.buildConnectionString().empty());
  CHECK(passed_params.credentials.getStorageAccountName() == "TEST_ACCOUNT");
  CHECK(passed_params.credentials.getEndpointSuffix() == "core.windows.net");
  CHECK(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setDynamicProperty(update_attribute_, "test.filesystemname", "");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  auto failed_flowfiles = getFailedFlowFileContents();
  CHECK(failed_flowfiles.size() == 1);
  CHECK(failed_flowfiles[0] == TEST_DATA);
  CHECK(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Delete file succeeds", "[azureDataLakeStorageDelete]") {
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().empty());
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedDeleteParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.filename == GETFILE_FILE_NAME);
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:filename value:" + GETFILE_FILE_NAME));
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Delete file fails", "[azureDataLakeStorageDelete]") {
  mock_data_lake_storage_client_ptr_->setDeleteFailure(true);
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedDeleteParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.filename == GETFILE_FILE_NAME);
  CHECK_FALSE(LogTestController::getInstance().contains("key:filename value:", 0s, 0ms));
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(DeleteAzureDataLakeStorageTestsFixture, "Delete result is false", "[azureDataLakeStorageDelete]") {
  mock_data_lake_storage_client_ptr_->setDeleteResult(false);
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedDeleteParams();
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.filename == GETFILE_FILE_NAME);
  CHECK_FALSE(LogTestController::getInstance().contains("key:filename value:", 0s, 0ms));
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

}  // namespace
