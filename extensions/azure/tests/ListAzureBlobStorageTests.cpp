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

#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "MockBlobStorage.h"
#include "unit/TestUtils.h"
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
    auto uuid = utils::IdGenerator::getIdGenerator()->generate();
    auto impl = std::make_unique<minifi::azure::processors::ListAzureBlobStorage>(
        core::ProcessorMetadata{
          .uuid = uuid, .name = "ListAzureBlobStorage",
          .logger = logging::LoggerFactory<minifi::azure::processors::ListAzureBlobStorage>::getLogger(uuid)}, std::move(mock_blob_storage));
    auto list_azure_blob_storage_unique_ptr = std::make_unique<core::Processor>(impl->getName(), impl->getUUID(), std::move(impl));
    list_azure_blob_storage_ = list_azure_blob_storage_unique_ptr.get();

    plan_->addProcessor(std::move(list_azure_blob_storage_unique_ptr), "ListAzureBlobStorage", { {"success", "d"} });
    auto logattribute = plan_->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);
    plan_->setProperty(logattribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

    azure_storage_cred_service_ = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
  }

  void setDefaultCredentials() {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
  }

  ListAzureBlobStorageTestsFixture(ListAzureBlobStorageTestsFixture&&) = delete;
  ListAzureBlobStorageTestsFixture(const ListAzureBlobStorageTestsFixture&) = delete;
  ListAzureBlobStorageTestsFixture& operator=(ListAzureBlobStorageTestsFixture&&) = delete;
  ListAzureBlobStorageTestsFixture& operator=(const ListAzureBlobStorageTestsFixture&) = delete;

  virtual ~ListAzureBlobStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockBlobStorage* mock_blob_storage_ptr_;
  core::Processor* list_azure_blob_storage_;
  std::shared_ptr<core::controller::ControllerServiceNode> azure_storage_cred_service_;
};

namespace {

using namespace std::literals::chrono_literals;

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

  SECTION("Account name and Azure default identity sources are used in properties") {
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

    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Credential Configuration Strategy", credential_configuration_strategy_string);
    plan_->setProperty(list_azure_blob_storage_, "Managed Identity Client ID", managed_identity_client_id);
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    CHECK(passed_params.credentials.buildConnectionString().empty());
    CHECK(passed_params.credentials.getStorageAccountName() == STORAGE_ACCOUNT_NAME);
    CHECK(passed_params.credentials.getEndpointSuffix() == "core.windows.net");
    CHECK(passed_params.credentials.getCredentialConfigurationStrategy() == expected_configuration_strategy_option);
    CHECK(passed_params.credentials.getManagedIdentityClientId() == managed_identity_client_id);
    CHECK(passed_params.container_name == CONTAINER_NAME);
  }

  SECTION("Account name and Azure default identity sources are used from Azure Storage Credentials Service") {
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

    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Credential Configuration Strategy", credential_configuration_strategy_string);
    plan_->setProperty(azure_storage_cred_service, "Common Storage Account Endpoint Suffix", "core.chinacloudapi.cn");
    plan_->setProperty(azure_storage_cred_service, "Managed Identity Client ID", managed_identity_client_id);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller_.runSession(plan_, true);
    auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
    CHECK(passed_params.credentials.buildConnectionString().empty());
    CHECK(passed_params.credentials.getStorageAccountName() == STORAGE_ACCOUNT_NAME);
    CHECK(passed_params.credentials.getEndpointSuffix() == "core.chinacloudapi.cn");
    CHECK(passed_params.credentials.getCredentialConfigurationStrategy() == expected_configuration_strategy_option);
    CHECK(passed_params.credentials.getManagedIdentityClientId() == managed_identity_client_id);
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

  SECTION("Both SAS Token and Storage Account Key cannot be set in credentials service") {
    auto azure_storage_cred_service = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan_->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_storage_cred_service, "SAS Token", SAS_TOKEN);
    plan_->setProperty(list_azure_blob_storage_, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }

  SECTION("Both SAS Token and Storage Account Key cannot be set in properties") {
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(list_azure_blob_storage_, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(list_azure_blob_storage_, "SAS Token", SAS_TOKEN);
    REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  }
}

TEST_CASE_METHOD(ListAzureBlobStorageTestsFixture, "List all files every time", "[ListAzureBlobStorage]") {
  setDefaultCredentials();
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ContainerName, CONTAINER_NAME);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::Prefix, PREFIX);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ListingStrategy, magic_enum::enum_name(minifi::azure::EntityTracking::none));
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
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
  LogTestController::getInstance().clear();
  test_controller_.runSession(plan_, true);
  run_assertions();
}

TEST_CASE_METHOD(ListAzureBlobStorageTestsFixture, "Do not list same files the second time when timestamps are tracked", "[ListAzureBlobStorage]") {
  setDefaultCredentials();
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ContainerName, CONTAINER_NAME);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::Prefix, PREFIX);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ListingStrategy, magic_enum::enum_name(minifi::azure::EntityTracking::timestamps));
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
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
  LogTestController::getInstance().clear();
  test_controller_.runSession(plan_, true);
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:azure", 0s, 0ms));
}

TEST_CASE_METHOD(ListAzureBlobStorageTestsFixture, "List all files through a proxy", "[ListAzureBlobStorage]") {
  setDefaultCredentials();
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ContainerName, CONTAINER_NAME);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::Prefix, PREFIX);
  plan_->setProperty(list_azure_blob_storage_, minifi::azure::processors::ListAzureBlobStorage::ListingStrategy, magic_enum::enum_name(minifi::azure::EntityTracking::none));

  auto proxy_configuration_service = plan_->addController("ProxyConfigurationService", "ProxyConfigurationService");
  plan_->setProperty(proxy_configuration_service, "Proxy Server Host", "host");
  plan_->setProperty(proxy_configuration_service, "Proxy Server Port", "1234");
  plan_->setProperty(proxy_configuration_service, "Proxy User Name", "username");
  plan_->setProperty(proxy_configuration_service, "Proxy User Password", "password");
  plan_->setProperty(proxy_configuration_service, "Proxy Type", "HTTP");
  plan_->setProperty(list_azure_blob_storage_, "Proxy Configuration Service", "ProxyConfigurationService");

  test_controller_.runSession(plan_, true);
  auto passed_params = mock_blob_storage_ptr_->getPassedListParams();
  REQUIRE(passed_params.proxy_configuration);
  REQUIRE(passed_params.proxy_configuration->proxy_host == "host");
  REQUIRE(passed_params.proxy_configuration->proxy_port);
  REQUIRE(*passed_params.proxy_configuration->proxy_port == 1234);
  REQUIRE(passed_params.proxy_configuration->proxy_user);
  REQUIRE(*passed_params.proxy_configuration->proxy_user == "username");
  REQUIRE(passed_params.proxy_configuration->proxy_password);
  REQUIRE(*passed_params.proxy_configuration->proxy_password == "password");
  REQUIRE(passed_params.proxy_configuration->proxy_type == minifi::controllers::ProxyType::HTTP);
}

}  // namespace
