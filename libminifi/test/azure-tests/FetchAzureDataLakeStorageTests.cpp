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
#include "processors/FetchAzureDataLakeStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"

namespace {

using namespace std::chrono_literals;

using FetchAzureDataLakeStorageTestsFixture = AzureDataLakeStorageTestsFixture<minifi::azure::processors::FetchAzureDataLakeStorage>;

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::FetchAzureDataLakeStorage::AzureStorageCredentialsService.getName(), "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().size() == 0);
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Test Azure credentials with account name and SAS token set", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken.getName(), "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName.getName(), "TEST_ACCOUNT");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), "");
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedFetchParams();
  REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=TEST_ACCOUNT;SharedAccessSignature=token");
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Test Azure credentials with connection string override", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), CONNECTION_STRING);
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken.getName(), "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName.getName(), "TEST_ACCOUNT");
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedFetchParams();
  REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Test Azure credentials with managed identity use", "[azureDataLakeStorageParameters]") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), "test");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::UseManagedIdentityCredentials.getName(), "true");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName.getName(), "TEST_ACCOUNT");
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedFetchParams();
  REQUIRE(passed_params.credentials.buildConnectionString().empty());
  REQUIRE(passed_params.credentials.getStorageAccountName() == "TEST_ACCOUNT");
  REQUIRE(passed_params.credentials.getEndpointSuffix() == "core.windows.net");
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(update_attribute_, "test.filesystemname", "", true);
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().size() == 0);
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Fetch full file succeeds", "[azureDataLakeStorageFetch]") {
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  auto success_contents = getSuccessfulFlowFileContents();
  REQUIRE(success_contents.size() == 1);
  REQUIRE(success_contents[0] == mock_data_lake_storage_client_ptr_->FETCHED_DATA);
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedFetchParams();
  REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  REQUIRE(passed_params.file_system_name == FILESYSTEM_NAME);
  REQUIRE(passed_params.directory_name == DIRECTORY_NAME);
  REQUIRE(passed_params.filename == GETFILE_FILE_NAME);
  REQUIRE(passed_params.range_start == std::nullopt);
  REQUIRE(passed_params.range_length == std::nullopt);
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Fetch a range of the file succeeds", "[azureDataLakeStorageFetch]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::FetchAzureDataLakeStorage::RangeStart.getName(), "5");
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::FetchAzureDataLakeStorage::RangeLength.getName(), "10");
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  auto success_contents = getSuccessfulFlowFileContents();
  REQUIRE(success_contents.size() == 1);
  REQUIRE(success_contents[0] == mock_data_lake_storage_client_ptr_->FETCHED_DATA.substr(5, 10));
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedFetchParams();
  REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  REQUIRE(passed_params.file_system_name == FILESYSTEM_NAME);
  REQUIRE(passed_params.directory_name == DIRECTORY_NAME);
  REQUIRE(passed_params.filename == GETFILE_FILE_NAME);
  REQUIRE(*passed_params.range_start == 5);
  REQUIRE(*passed_params.range_length == 10);
}

TEST_CASE_METHOD(FetchAzureDataLakeStorageTestsFixture, "Number of Retries is set", "[azureDataLakeStorageFetch]") {
  plan_->setProperty(azure_data_lake_storage_, minifi::azure::processors::FetchAzureDataLakeStorage::NumberOfRetries.getName(), "1");
  test_controller_.runSession(plan_, true);
  REQUIRE(mock_data_lake_storage_client_ptr_->getPassedFetchParams().number_of_retries == 1);
}

}  // namespace
