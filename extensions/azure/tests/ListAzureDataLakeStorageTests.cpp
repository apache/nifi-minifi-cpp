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
#include "MockDataLakeStorageClient.h"
#include "unit/TestUtils.h"
#include "processors/LogAttribute.h"
#include "processors/ListAzureDataLakeStorage.h"
#include "controllerservices/AzureStorageCredentialsService.h"

const std::string FILESYSTEM_NAME = "testfilesystem";
const std::string DIRECTORY_NAME = "testdir";
const std::string CONNECTION_STRING = "test-connectionstring";

class ListAzureDataLakeStorageTestsFixture {
 public:
  ListAzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setDebug<minifi::utils::ListingStateManager>();
    LogTestController::getInstance().setTrace<minifi::azure::processors::ListAzureDataLakeStorage>();

    // Build MiNiFi processing graph
    plan_ = test_controller_.createPlan();
    auto mock_data_lake_storage_client = std::make_unique<MockDataLakeStorageClient>();
    mock_data_lake_storage_client_ptr_ = mock_data_lake_storage_client.get();
    auto uuid = utils::IdGenerator::getIdGenerator()->generate();
    auto impl = std::unique_ptr<minifi::azure::processors::ListAzureDataLakeStorage>(  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
      new minifi::azure::processors::ListAzureDataLakeStorage({
        .uuid = uuid, .name = "ListAzureDataLakeStorage",
        .logger = logging::LoggerFactory<minifi::azure::processors::ListAzureDataLakeStorage>::getLogger(uuid)}, std::move(mock_data_lake_storage_client)));
    auto list_azure_data_lake_storage_unique_ptr = std::make_unique<core::Processor>(impl->getName(), impl->getUUID(), std::move(impl));
    list_azure_data_lake_storage_ = list_azure_data_lake_storage_unique_ptr.get();

    plan_->addProcessor(std::move(list_azure_data_lake_storage_unique_ptr), "ListAzureDataLakeStorage", { {"success", "d"} });
    auto logattribute = plan_->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);
    plan_->setProperty(logattribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");

    azure_storage_cred_service_ = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    setDefaultProperties();
  }

  void setDefaultProperties() {
    plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::AzureStorageCredentialsService, "AzureStorageCredentialsService");
    plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::FilesystemName, FILESYSTEM_NAME);
    plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::DirectoryName, DIRECTORY_NAME);
    plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, CONNECTION_STRING);
  }

  ListAzureDataLakeStorageTestsFixture(ListAzureDataLakeStorageTestsFixture&&) = delete;
  ListAzureDataLakeStorageTestsFixture(const ListAzureDataLakeStorageTestsFixture&) = delete;
  ListAzureDataLakeStorageTestsFixture& operator=(ListAzureDataLakeStorageTestsFixture&&) = delete;
  ListAzureDataLakeStorageTestsFixture& operator=(const ListAzureDataLakeStorageTestsFixture&) = delete;

  virtual ~ListAzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockDataLakeStorageClient* mock_data_lake_storage_client_ptr_;
  core::Processor* list_azure_data_lake_storage_;
  std::shared_ptr<core::controller::ControllerServiceNode> azure_storage_cred_service_;
};

namespace {

using namespace std::literals::chrono_literals;

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::AzureStorageCredentialsService, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::FilesystemName, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "List all files every time", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::ListingStrategy, magic_enum::enum_name(minifi::azure::EntityTracking::none));
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::RecurseSubdirectories, "false");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  auto run_assertions = [this]() {
    auto passed_params = mock_data_lake_storage_client_ptr_->getPassedListParams();
    CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
    CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
    CHECK(passed_params.directory_name == DIRECTORY_NAME);
    CHECK(passed_params.recurse_subdirectories == false);
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filePath value:testdir/item1.log"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filePath value:testdir/sub/item2.log"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:item1.log"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:item2.log"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:128"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:256"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag1"));
    CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag2"));
  };
  run_assertions();
  plan_->reset();
  LogTestController::getInstance().clear();
  test_controller_.runSession(plan_, true);
  run_assertions();
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Do not list same files the second time when timestamps are tracked", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::ListingStrategy, magic_enum::enum_name(minifi::azure::EntityTracking::timestamps));
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::RecurseSubdirectories, "false");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedListParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.recurse_subdirectories == false);
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filePath value:testdir/item1.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filePath value:testdir/sub/item2.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:item1.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:item2.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:128"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:256"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag1"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag2"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.lastModified value:" + mock_data_lake_storage_client_ptr_->ITEM1_LAST_MODIFIED + "\n"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.lastModified value:" + mock_data_lake_storage_client_ptr_->ITEM2_LAST_MODIFIED + "\n"));
  plan_->reset();
  LogTestController::getInstance().clear();
  test_controller_.runSession(plan_, true);
  REQUIRE_FALSE(LogTestController::getInstance().contains("key:azure", 0s, 0ms));
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Do not list filtered files", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::FileFilter, "item1.*g");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedListParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.recurse_subdirectories == true);
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filePath value:testdir/item1.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:item1.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:128"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag1"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.lastModified value:" + mock_data_lake_storage_client_ptr_->ITEM1_LAST_MODIFIED + "\n"));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.filePath value:testdir/sub/item2.log", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.filename value:item2.log", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.length value:256", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.etag value:etag2", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.lastModified value:" + mock_data_lake_storage_client_ptr_->ITEM2_LAST_MODIFIED, 0s, 0ms));
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Do not list filtered paths", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::PathFilter, "su.*");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedListParams();
  CHECK(passed_params.credentials.buildConnectionString() == CONNECTION_STRING);
  CHECK(passed_params.file_system_name == FILESYSTEM_NAME);
  CHECK(passed_params.directory_name == DIRECTORY_NAME);
  CHECK(passed_params.recurse_subdirectories == true);
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filePath value:testdir/sub/item2.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:item2.log"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:256"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.etag value:etag2"));
  CHECK(verifyLogLinePresenceInPollTime(1s, "key:azure.lastModified value:" + mock_data_lake_storage_client_ptr_->ITEM2_LAST_MODIFIED + "\n"));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.filePath value:testdir/item1.log", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.filename value:item1.log", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.length value:128", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.etag value:etag1", 0s, 0ms));
  CHECK_FALSE(LogTestController::getInstance().contains("key:azure.lastModified value:" + mock_data_lake_storage_client_ptr_->ITEM1_LAST_MODIFIED, 0s, 0ms));
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Throw on invalid file filter", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::FileFilter, "(item1][].*g");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Throw on invalid path filter", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::PathFilter, "su.([[*");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Test Azure credentials with Azure default identity sources", "[azureDataLakeStorageParameters]") {
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
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedListParams();
  CHECK(passed_params.credentials.buildConnectionString().empty());
  CHECK(passed_params.credentials.getStorageAccountName() == "TEST_ACCOUNT");
  CHECK(passed_params.credentials.getEndpointSuffix() == "core.windows.net");
  CHECK(passed_params.credentials.getCredentialConfigurationStrategy() == expected_configuration_strategy_option);
  CHECK(passed_params.credentials.getManagedIdentityClientId() == managed_identity_client_id);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Both SAS Token and Storage Account Key cannot be set in credentials service") {
  setDefaultProperties();
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::SASToken, "token");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountName, "TEST_ACCOUNT");
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::StorageAccountKey, "TEST_KEY");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "List data lake storage files using proxy", "[azureDataLakeStorageParameters]") {
  auto proxy_configuration_service = plan_->addController("ProxyConfigurationService", "ProxyConfigurationService");
  plan_->setProperty(proxy_configuration_service, "Proxy Server Host", "host");
  plan_->setProperty(proxy_configuration_service, "Proxy Server Port", "1234");
  plan_->setProperty(proxy_configuration_service, "Proxy User Name", "username");
  plan_->setProperty(proxy_configuration_service, "Proxy User Password", "password");
  plan_->setProperty(list_azure_data_lake_storage_, "Proxy Configuration Service", "ProxyConfigurationService");

  test_controller_.runSession(plan_, true);

  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedListParams();
  REQUIRE(passed_params.proxy_configuration);
  REQUIRE(passed_params.proxy_configuration->proxy_host == "host");
  REQUIRE(passed_params.proxy_configuration->proxy_port);
  REQUIRE(*passed_params.proxy_configuration->proxy_port == 1234);
  REQUIRE(passed_params.proxy_configuration->proxy_user);
  REQUIRE(*passed_params.proxy_configuration->proxy_user == "username");
  REQUIRE(passed_params.proxy_configuration->proxy_password);
  REQUIRE(*passed_params.proxy_configuration->proxy_password == "password");
}

}  // namespace
