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

#include "../TestBase.h"
#include "../Catch.h"
#include "MockBlobStorage.h"
#include "utils/IntegrationTestUtils.h"
#include "processors/LogAttribute.h"
#include "processors/ListAzureBlobStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"

const std::string CONTAINER_NAME = "test-container";
const std::string STORAGE_ACCOUNT_NAME = "test-account";
const std::string STORAGE_ACCOUNT_KEY = "test-key";
const std::string SAS_TOKEN = "test-sas-token";
const std::string ENDPOINT_SUFFIX = "test.suffix.com";
const std::string CONNECTION_STRING = "test-connectionstring";
const std::string PREFIX = "test_prefix";

class ListAzureBlobStorageTestsFixture {
 public:
  ListAzureBlobStorageTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::azure::processors::ListAzureBlobStorage>();

    // Build MiNiFi processing graph
    plan_ = test_controller_.createPlan();
    auto mock_blob_storage = std::make_unique<MockBlobStorage>();
    mock_blob_storage_ptr_ = mock_blob_storage.get();
    list_azure_blob_storage_ = std::make_shared<minifi::azure::processors::ListAzureBlobStorage>("ListAzureBlobStorage", std::move(mock_blob_storage));

    plan_->addProcessor(list_azure_blob_storage_, "ListAzureBlobStorage", { {"success", "d"} });
    auto logattribute = plan_->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);
    plan_->setProperty(logattribute, minifi::processors::LogAttribute::FlowFilesToLog.getName(), "0");

    azure_storage_cred_service_ = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
  }

  void setDefaultCredentials() {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
  }

  virtual ~ListAzureBlobStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockBlobStorage* mock_blob_storage_ptr_;
  std::shared_ptr<core::Processor> list_azure_blob_storage_;
  std::shared_ptr<core::controller::ControllerServiceNode> azure_storage_cred_service_;
};

namespace {

using namespace std::chrono_literals;

TEST_CASE_METHOD(ListAzureBlobStorageTestsFixture, "Test credentials settings", "[azureStorageCredentials]") {
  plan_->setProperty(list_azure_blob_storage_, "Container Name", CONTAINER_NAME);

  SECTION("No credentials are set") {
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }

  SECTION("No account key or SAS is set") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }

  SECTION("Credentials set in Azure Storage Credentials Service") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Overriding credentials set in Azure Storage Credentials Service with connection string") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_storage_cred_service, "Connection String", CONNECTION_STRING);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  }

  SECTION("Account name and key set in properties") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Account name and SAS token set in properties") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "SAS Token", SAS_TOKEN);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";SharedAccessSignature=" + SAS_TOKEN);
  }

  SECTION("Account name and SAS token with question mark set in properties") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "SAS Token", "?" + SAS_TOKEN);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";SharedAccessSignature=" + SAS_TOKEN);
  }

  SECTION("Endpoint suffix overriden") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "Common Storage Account Endpoint Suffix", ENDPOINT_SUFFIX);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY + ";EndpointSuffix=" + ENDPOINT_SUFFIX);
  }

  SECTION("Use connection string") {
    plan_->setProperty(list_azure_blob_storage_, "Connection String", CONNECTION_STRING);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  }

  SECTION("Overriding credentials with connection string") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "Connection String", CONNECTION_STRING);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  }

  SECTION("Account name and managed identity are used in properties") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Use Managed Identity Credentials", "true");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
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
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    CHECK(passed_params.credentials.buildConnectionString().empty());
    CHECK(passed_params.credentials.getStorageAccountName() == STORAGE_ACCOUNT_NAME);
    CHECK(passed_params.credentials.getEndpointSuffix() == "core.chinacloudapi.cn");
    CHECK(passed_params.container_name == CONTAINER_NAME);
  }

  SECTION("Azure Storage Credentials Service overrides properties") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "Connection String", CONNECTION_STRING);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    REQUIRE(passed_params.credentials.buildConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Azure Storage Credentials Service is set with invalid parameters") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }

  SECTION("Azure Storage Credentials Service name is invalid") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "invalid_name");
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }
}

TEST_CASE_METHOD(ListAzureBlobStorageTestsFixture, "List all files every time", "[ListAzureBlobStorage]") {
  setDefaultCredentials();
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ContainerName.getName(), CONTAINER_NAME);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::Prefix.getName(), PREFIX);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ListingStrategy.getName(),
    toString(minifi::azure::processors::ListAzureBlobStorage::EntityTracking::NONE));
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  auto run_assertions = [this]() {
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    CHECK(passed_params.container_name == CONTAINER_NAME);
    CHECK(passed_params.prefix == PREFIX);
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.container value:" + CONTAINER_NAME));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobname value:testdir/item1.log"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobname value:testdir/item2.log"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.primaryUri value:" + mock_blob_storage_ptr_->PRIMARY_URI));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:128"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:256"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag1"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag2"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.timestamp value:" + mock_blob_storage_ptr_->ITEM1_LAST_MODIFIED));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.timestamp value:" + mock_blob_storage_ptr_->ITEM2_LAST_MODIFIED));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobtype value:PageBlob"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobtype value:BlockBlob"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:mime.type value:application/zip"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:mime.type value:text/html"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:lang value:en-US"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:lang value:de-DE"));
  };
  run_assertions();
  plan_->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
  test_controller_.runSession(plan_, true);
  run_assertions();
}

TEST_CASE_METHOD(ListAzureBlobStorageTestsFixture, "Do not list same files the second time when timestamps are tracked", "[ListAzureBlobStorage]") {
  setDefaultCredentials();
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ContainerName.getName(), CONTAINER_NAME);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::Prefix.getName(), PREFIX);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ListingStrategy.getName(),
    toString(minifi::azure::processors::ListAzureBlobStorage::EntityTracking::TIMESTAMPS));
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
  CHECK(passed_params.container_name == CONTAINER_NAME);
  CHECK(passed_params.prefix == PREFIX);
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.container value:" + CONTAINER_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobname value:testdir/item1.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobname value:testdir/item2.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.primaryUri value:" + mock_blob_storage_ptr_->PRIMARY_URI));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:128"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:256"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag1"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag2"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.timestamp value:" + mock_blob_storage_ptr_->ITEM1_LAST_MODIFIED));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.timestamp value:" + mock_blob_storage_ptr_->ITEM2_LAST_MODIFIED));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobtype value:PageBlob"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.blobtype value:BlockBlob"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:mime.type value:application/zip"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:mime.type value:text/html"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:lang value:en-US"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:lang value:de-DE"));
  plan_->reset();
  LogTestController::getInstance().resetStream(LogTestController::getInstance().log_output);
  test_controller_.runSession(plan_, true);
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:azure", 0s, 0ms));
}

}  // namespace
