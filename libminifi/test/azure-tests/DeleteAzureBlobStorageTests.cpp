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

#include "AzureBlobStorageTestsFixture.h"
#include "processors/DeleteAzureBlobStorage.h"

namespace {

using DeleteAzureBlobStorageTestsFixture = AzureBlobStorageTestsFixture<minifi::azure::processors::DeleteAzureBlobStorage>;

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Container name not set", "[azureStorageParameters]") {
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Test credentials settings", "[azureStorageCredentials]") {
  plan_->setDynamicProperty(update_attribute_processor_, "test.container", CONTAINER_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Container Name", "${test.container}");
  plan_->setDynamicProperty(update_attribute_processor_, "test.blob", BLOB_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Blob", "${test.blob}");

  SECTION("No credentials are set") {
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }

  SECTION("No account key or SAS is set") {
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }

  SECTION("Credentials set in Azure Storage Credentials Service") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Overriding credentials set in Azure Storage Credentials Service with connection string") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_storage_cred_service, "Connection String", CONNECTION_STRING);
    plan_->setProperty(azure_blob_storage_processor_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  }

  SECTION("Account name and key set in properties") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Account name and SAS token set in properties") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.sas_token", SAS_TOKEN);
    plan_->setProperty(azure_blob_storage_processor_, "SAS Token", "${test.sas_token}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";SharedAccessSignature=" + SAS_TOKEN);
  }

  SECTION("Account name and SAS token with question mark set in properties") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.sas_token", "?" + SAS_TOKEN);
    plan_->setProperty(azure_blob_storage_processor_, "SAS Token", "${test.sas_token}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";SharedAccessSignature=" + SAS_TOKEN);
  }

  SECTION("Endpoint suffix overriden") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.endpoint_suffix", ENDPOINT_SUFFIX);
    plan_->setProperty(azure_blob_storage_processor_, "Common Storage Account Endpoint Suffix", "${test.endpoint_suffix}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY + ";EndpointSuffix=" + ENDPOINT_SUFFIX);
  }

  SECTION("Use connection string") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.connection_string", CONNECTION_STRING);
    plan_->setProperty(azure_blob_storage_processor_, "Connection String", "${test.connection_string}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  }

  SECTION("Overriding credentials with connection string") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.connection_string", CONNECTION_STRING);
    plan_->setProperty(azure_blob_storage_processor_, "Connection String", "${test.connection_string}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  }

  SECTION("Connection string is empty after substituting it from expression language") {
    plan_->setDynamicProperty(update_attribute_processor_, "test.connection_string", "");
    plan_->setProperty(azure_blob_storage_processor_, "Connection String", "${test.connection_string}");
    test_controller_.runSession(plan_, true);
    auto failed_flowfiles = getFailedFlowFileContents();
    REQUIRE(failed_flowfiles.size() == 1);
    REQUIRE(failed_flowfiles[0] == TEST_DATA);
  }

  SECTION("Account name and managed identity are used in properties") {
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Use Managed Identity Credentials", "true");
    test_controller_.runSession(plan_, true);
    CHECK(getFailedFlowFileContents().empty());
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    CHECK(passed_params.credentials.buildConnectionString().empty());
    CHECK(passed_params.credentials.getStorageAccountName() == STORAGE_ACCOUNT_NAME);
    CHECK(passed_params.credentials.getEndpointSuffix() == "core.windows.net");
    CHECK(passed_params.container_name == CONTAINER_NAME);
  }

  SECTION("Account name and managed identity are used from Azure Storage Credentials Service") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Use Managed Identity Credentials", "true");
    plan_->setProperty(azure_storage_cred_service, "Common Storage Account Endpoint Suffix", "core.chinacloudapi.cn");
    plan_->setProperty(azure_blob_storage_processor_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    CHECK(getFailedFlowFileContents().empty());
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    CHECK(passed_params.credentials.buildConnectionString().empty());
    CHECK(passed_params.credentials.getStorageAccountName() == STORAGE_ACCOUNT_NAME);
    CHECK(passed_params.credentials.getEndpointSuffix() == "core.chinacloudapi.cn");
    CHECK(passed_params.container_name == CONTAINER_NAME);
  }

  SECTION("Azure Storage Credentials Service overrides properties") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.connection_string", CONNECTION_STRING);
    plan_->setProperty(azure_blob_storage_processor_, "Connection String", "${test.connection_string}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Azure Storage Credentials Service is set with invalid parameters") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString().empty());
    auto failed_flowfiles = getFailedFlowFileContents();
    REQUIRE(failed_flowfiles.size() == 1);
    REQUIRE(failed_flowfiles[0] == TEST_DATA);
  }

  SECTION("Azure Storage Credentials Service name is invalid") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Azure Storage Credentials Service", "invalid_name");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
    REQUIRE(passed_params.credentials.buildConnectionString().empty());
    auto failed_flowfiles = getFailedFlowFileContents();
    REQUIRE(failed_flowfiles.size() == 1);
    REQUIRE(failed_flowfiles[0] == TEST_DATA);
  }
}

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Test Azure blob delete failure in case Blob is not set and filename is empty", "[azureBlobStorageDelete]") {
  plan_->setDynamicProperty(update_attribute_processor_, "test.container", CONTAINER_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Container Name", "${test.container}");
  plan_->setDynamicProperty(update_attribute_processor_, "filename", "");
  setDefaultCredentials();
  test_controller_.runSession(plan_, true);
  REQUIRE(LogTestController::getInstance().contains("Blob is not set and default 'filename' attribute could not be found!"));
}

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Test Azure blob delete with default blob name", "[azureBlobStorageDelete]") {
  plan_->setDynamicProperty(update_attribute_processor_, "test.container", CONTAINER_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Container Name", "${test.container}");
  setDefaultCredentials();
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
  CHECK(passed_params.container_name == CONTAINER_NAME);
  CHECK(passed_params.blob_name == GET_FILE_NAME);
  CHECK(passed_params.optional_deletion == minifi::azure::storage::OptionalDeletion::NONE);
  CHECK(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Test Azure blob delete including snapshots", "[azureBlobStorageDelete]") {
  plan_->setDynamicProperty(update_attribute_processor_, "test.container", CONTAINER_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Container Name", "${test.container}");
  plan_->setDynamicProperty(update_attribute_processor_, "test.blob", BLOB_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Blob", "${test.blob}");
  plan_->setProperty(azure_blob_storage_processor_, "Delete Snapshots Option", "Include Snapshots");
  setDefaultCredentials();
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
  CHECK(passed_params.container_name == CONTAINER_NAME);
  CHECK(passed_params.blob_name == BLOB_NAME);
  CHECK(passed_params.optional_deletion == minifi::azure::storage::OptionalDeletion::INCLUDE_SNAPSHOTS);
  CHECK(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Test Azure blob delete with snapshots only", "[azureBlobStorageDelete]") {
  plan_->setDynamicProperty(update_attribute_processor_, "test.container", CONTAINER_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Container Name", "${test.container}");
  plan_->setDynamicProperty(update_attribute_processor_, "test.blob", BLOB_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Blob", "${test.blob}");
  plan_->setProperty(azure_blob_storage_processor_, "Delete Snapshots Option", "Delete Snapshots Only");
  setDefaultCredentials();
  test_controller_.runSession(plan_, true);
  auto passed_params = mock_blob_storage_ptr_->getPassedDeleteParams();
  CHECK(passed_params.container_name == CONTAINER_NAME);
  CHECK(passed_params.blob_name == BLOB_NAME);
  CHECK(passed_params.optional_deletion == minifi::azure::storage::OptionalDeletion::DELETE_SNAPSHOTS_ONLY);
  CHECK(getFailedFlowFileContents().empty());
}

TEST_CASE_METHOD(DeleteAzureBlobStorageTestsFixture, "Test Azure blob delete with remote failure", "[azureBlobStorageDelete]") {
  plan_->setDynamicProperty(update_attribute_processor_, "test.container", CONTAINER_NAME);
  plan_->setProperty(azure_blob_storage_processor_, "Container Name", "${test.container}");
  mock_blob_storage_ptr_->setDeleteFailure(true);
  setDefaultCredentials();
  test_controller_.runSession(plan_, true);
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

}  // namespace
