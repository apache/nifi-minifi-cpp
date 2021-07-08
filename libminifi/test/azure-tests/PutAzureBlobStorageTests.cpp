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
#include "core/Processor.h"
#include "processors/PutAzureBlobStorage.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "storage/BlobStorage.h"
#include "utils/file/FileUtils.h"

const std::string CONTAINER_NAME = "test-container";
const std::string STORAGE_ACCOUNT_NAME = "test-account";
const std::string STORAGE_ACCOUNT_KEY = "test-key";
const std::string SAS_TOKEN = "test-sas-token";
const std::string ENDPOINT_SUFFIX = "test.suffix.com";
const std::string CONNECTION_STRING = "test-connectionstring";
const std::string BLOB_NAME = "test-blob.txt";
const std::string TEST_DATA = "data";

class MockBlobStorage : public minifi::azure::storage::BlobStorage {
 public:
  const std::string ETAG = "test-etag";
  const std::string PRIMARY_URI = "test-uri";
  const std::string TEST_TIMESTAMP = "test-timestamp";

  MockBlobStorage()
    : BlobStorage("", "") {
  }

  void createContainer() override {
    container_created_ = true;
  }

  void resetClientIfNeeded(const std::string &connection_string, const std::string &container_name) override {
    connection_string_ = connection_string;
    container_name_ = container_name;
  }

  utils::optional<minifi::azure::storage::UploadBlobResult> uploadBlob(const std::string& /*blob_name*/, const uint8_t* buffer, std::size_t buffer_size) override {
    input_data = std::string(buffer, buffer + buffer_size);
    minifi::azure::storage::UploadBlobResult result;
    result.etag = ETAG;
    result.length = buffer_size;
    result.primary_uri = PRIMARY_URI;
    result.timestamp = TEST_TIMESTAMP;
    return result;
  }

  std::string getConnectionString() const {
    return connection_string_;
  }

  std::string getContainerName() const {
    return container_name_;
  }

  bool getContainerCreated() const {
    return container_created_;
  }

  std::string input_data;

 private:
  bool container_created_ = false;
};

class PutAzureBlobStorageTestsFixture {
 public:
  PutAzureBlobStorageTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<processors::GetFile>();
    LogTestController::getInstance().setDebug<processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::azure::processors::PutAzureBlobStorage>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    auto mock_blob_storage = utils::make_unique<MockBlobStorage>();
    mock_blob_storage_ptr = mock_blob_storage.get();
    put_azure_blob_storage = std::shared_ptr<minifi::azure::processors::PutAzureBlobStorage>(
      new minifi::azure::processors::PutAzureBlobStorage("PutAzureBlobStorage", utils::Identifier(), std::move(mock_blob_storage)));
    char input_dir_mask[] = "/tmp/gt.XXXXXX";
    auto input_dir = test_controller.createTempDirectory(input_dir_mask);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + "input_data.log");
    input_file_stream << TEST_DATA;
    input_file_stream.close();
    get_file = plan->addProcessor("GetFile", "GetFile");
    plan->setProperty(get_file, processors::GetFile::Directory.getName(), input_dir);
    plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "false");
    update_attribute = plan->addProcessor("UpdateAttribute", "UpdateAttribute", { {"success", "d"} },  true);
    plan->addProcessor(put_azure_blob_storage, "PutAzureBlobStorage", { {"success", "d"} }, true);
    plan->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);
  }

  void setDefaultCredentials() {
    plan->setProperty(update_attribute, "test.account_name", STORAGE_ACCOUNT_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", "${test.account_name}");
    plan->setProperty(update_attribute, "test.account_key", STORAGE_ACCOUNT_KEY, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Key", "${test.account_key}");
  }

  virtual ~PutAzureBlobStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  MockBlobStorage* mock_blob_storage_ptr;
  std::shared_ptr<core::Processor> put_azure_blob_storage;
  std::shared_ptr<core::Processor> get_file;
  std::shared_ptr<core::Processor> update_attribute;
};

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test required parameters", "[azureStorageParameters]") {
  SECTION("Container name not set") {
  }

  SECTION("Blob name not set") {
    plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test credentials settings", "[azureStorageCredentials]") {
  plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");

  SECTION("No credentials are set") {
    REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
  }

  SECTION("No account key or SAS is set") {
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
  }

  SECTION("Credentials set in Azure Storage Credentials Service") {
    auto azure_storage_cred_service = plan->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan->setProperty(put_azure_blob_storage, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Overriding credentials set in Azure Storage Credentials Service with connection string") {
    auto azure_storage_cred_service = plan->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan->setProperty(azure_storage_cred_service, "Connection String", CONNECTION_STRING);
    plan->setProperty(put_azure_blob_storage, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == CONNECTION_STRING);
  }

  SECTION("Account name and key set in properties") {
    plan->setProperty(update_attribute, "test.account_name", STORAGE_ACCOUNT_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", "${test.account_name}");
    plan->setProperty(update_attribute, "test.account_key", STORAGE_ACCOUNT_KEY, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Key", "${test.account_key}");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY);
  }

  SECTION("Account name and SAS token set in properties") {
    plan->setProperty(update_attribute, "test.account_name", STORAGE_ACCOUNT_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", "${test.account_name}");
    plan->setProperty(update_attribute, "test.sas_token", SAS_TOKEN, true);
    plan->setProperty(put_azure_blob_storage, "SAS Token", "${test.sas_token}");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";SharedAccessSignature=" + SAS_TOKEN);
  }

  SECTION("Account name and SAS token with question mark set in properties") {
    plan->setProperty(update_attribute, "test.account_name", STORAGE_ACCOUNT_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", "${test.account_name}");
    plan->setProperty(update_attribute, "test.sas_token", "?" + SAS_TOKEN, true);
    plan->setProperty(put_azure_blob_storage, "SAS Token", "${test.sas_token}");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";SharedAccessSignature=" + SAS_TOKEN);
  }

  SECTION("Endpoint suffix overriden") {
    plan->setProperty(update_attribute, "test.account_name", STORAGE_ACCOUNT_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", "${test.account_name}");
    plan->setProperty(update_attribute, "test.account_key", STORAGE_ACCOUNT_KEY, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Key", "${test.account_key}");
    plan->setProperty(update_attribute, "test.endpoint_suffix", ENDPOINT_SUFFIX, true);
    plan->setProperty(put_azure_blob_storage, "Common Storage Account Endpoint Suffix", "${test.endpoint_suffix}");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == "AccountName=" + STORAGE_ACCOUNT_NAME + ";AccountKey=" + STORAGE_ACCOUNT_KEY + ";EndpointSuffix=" + ENDPOINT_SUFFIX);
  }

  SECTION("Use connection string") {
    plan->setProperty(update_attribute, "test.connection_string", CONNECTION_STRING, true);
    plan->setProperty(put_azure_blob_storage, "Connection String", "${test.connection_string}");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == CONNECTION_STRING);
  }

  SECTION("Overriding credentials with connection string") {
    auto azure_storage_cred_service = plan->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    plan->setProperty(azure_storage_cred_service, "Storage Account Name", STORAGE_ACCOUNT_NAME);
    plan->setProperty(azure_storage_cred_service, "Storage Account Key", STORAGE_ACCOUNT_KEY);
    plan->setProperty(put_azure_blob_storage, "Azure Storage Credentials Service", "AzureStorageCredentialsService");
    plan->setProperty(update_attribute, "test.account_name", STORAGE_ACCOUNT_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Name", "${test.account_name}");
    plan->setProperty(update_attribute, "test.account_key", STORAGE_ACCOUNT_KEY, true);
    plan->setProperty(put_azure_blob_storage, "Storage Account Key", "${test.account_key}");
    plan->setProperty(update_attribute, "test.connection_string", CONNECTION_STRING, true);
    plan->setProperty(put_azure_blob_storage, "Connection String", "${test.connection_string}");
    test_controller.runSession(plan, true);
    REQUIRE(mock_blob_storage_ptr->getConnectionString() == CONNECTION_STRING);
  }
}

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test Azure blob upload", "[azureBlobStorageUpload]") {
  plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");
  setDefaultCredentials();
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:azure.container value:" + CONTAINER_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.blobname value:" + BLOB_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.primaryUri value:" + mock_blob_storage_ptr->PRIMARY_URI));
  REQUIRE(LogTestController::getInstance().contains("key:azure.etag value:" + mock_blob_storage_ptr->ETAG));
  REQUIRE(LogTestController::getInstance().contains("key:azure.length value:" + std::to_string(TEST_DATA.size())));
  REQUIRE(LogTestController::getInstance().contains("key:azure.timestamp value:" + mock_blob_storage_ptr->TEST_TIMESTAMP));
  REQUIRE(mock_blob_storage_ptr->input_data == TEST_DATA);
  REQUIRE(mock_blob_storage_ptr->getContainerCreated() == false);
  REQUIRE(mock_blob_storage_ptr->getContainerName() == CONTAINER_NAME);
}

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test Azure blob upload with container creation", "[azureBlobStorageUpload]") {
  plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");
  plan->setProperty(put_azure_blob_storage, "Create Container", "true");
  setDefaultCredentials();
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:azure.container value:" + CONTAINER_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.blobname value:" + BLOB_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.primaryUri value:" + mock_blob_storage_ptr->PRIMARY_URI));
  REQUIRE(LogTestController::getInstance().contains("key:azure.etag value:" + mock_blob_storage_ptr->ETAG));
  REQUIRE(LogTestController::getInstance().contains("key:azure.length value:" + std::to_string(TEST_DATA.size())));
  REQUIRE(LogTestController::getInstance().contains("key:azure.timestamp value:" + mock_blob_storage_ptr->TEST_TIMESTAMP));
  REQUIRE(mock_blob_storage_ptr->input_data == TEST_DATA);
  REQUIRE(mock_blob_storage_ptr->getContainerCreated() == true);
  REQUIRE(mock_blob_storage_ptr->getContainerName() == CONTAINER_NAME);
}
