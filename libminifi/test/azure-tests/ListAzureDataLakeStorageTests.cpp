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
#include "MockDataLakeStorageClient.h"
#include "utils/IntegrationTestUtils.h"
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
    list_azure_data_lake_storage_ = std::shared_ptr<minifi::azure::processors::ListAzureDataLakeStorage>(
      new minifi::azure::processors::ListAzureDataLakeStorage("ListAzureDataLakeStorage", utils::Identifier(), std::move(mock_data_lake_storage_client)));

    plan_->addProcessor(list_azure_data_lake_storage_, "ListAzureDataLakeStorage", { {"success", "d"} });
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

  virtual ~ListAzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockDataLakeStorageClient* mock_data_lake_storage_client_ptr_;
  std::shared_ptr<core::Processor> list_azure_data_lake_storage_;
  std::shared_ptr<core::controller::ControllerServiceNode> azure_storage_cred_service_;
};

namespace {

using namespace std::chrono_literals;

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::AzureStorageCredentialsService, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::FilesystemName, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
}

TEST_CASE_METHOD(ListAzureDataLakeStorageTestsFixture, "List all files every time", "[listAzureDataLakeStorage]") {
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::ListingStrategy,
    toString(minifi::azure::processors::azure::EntityTracking::NONE));
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::RecurseSubdirectories, "false");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
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
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::ListingStrategy,
    toString(minifi::azure::processors::azure::EntityTracking::TIMESTAMPS));
  plan_->setProperty(list_azure_data_lake_storage_, minifi::azure::processors::ListAzureDataLakeStorage::RecurseSubdirectories, "false");
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
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
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
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
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
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

}  // namespace
