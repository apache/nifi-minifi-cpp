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

namespace {

const std::string CONTAINER_NAME = "test-container";
const std::string CONNECTION_STRING = "test-connectionstring";
const std::string AZURE_CREDENTIALS_SERVICE_NAME = "AzureCredentialsService";
const std::string BLOB_NAME = "test-blob.txt";
const std::string TEST_DATA = "data";

class MockBlobStorage : public minifi::azure::storage::BlobStorage {
 public:
  const std::string ETAG = "test-etag";
  const std::string PRIMARY_URI = "test-uri";
  const std::string TEST_TIMESTAMP = "test-timestamp";

  MockBlobStorage(const std::string &connection_string, const std::string &container_name)
    : BlobStorage(connection_string, container_name) {
  }

  void createContainer() override {
    container_created_ = true;
  }

  void resetClientIfNeeded(const std::string &connection_string, const std::string &container_name) override {
    connection_string_ = connection_string;
    container_name_ = container_name;
  }

  utils::optional<minifi::azure::storage::UploadBlobResult> uploadBlob(const std::string &blob_name, const uint8_t* buffer, std::size_t buffer_size) override {
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
    mock_blob_storage_ptr = new MockBlobStorage("", "");
    std::unique_ptr<minifi::azure::storage::BlobStorage> mock_blob_storage(mock_blob_storage_ptr);
    put_azure_blob_storage = std::make_shared<minifi::azure::processors::PutAzureBlobStorage>("PutAzureBlobStorage", utils::Identifier(), std::move(mock_blob_storage));

    char input_dir_mask[] = "/tmp/gt.XXXXXX";
    auto input_dir = test_controller.createTempDirectory(input_dir_mask);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + "input_data.log");
    input_file_stream << TEST_DATA;
    input_file_stream.close();
    get_file = plan->addProcessor("GetFile", "GetFile");
    plan->setProperty(get_file, processors::GetFile::Directory.getName(), input_dir);
    plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "false");
    update_attribute = plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      put_azure_blob_storage,
      "PutAzureBlobStorage",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
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

  SECTION("No connection string is set") {
    plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
    plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
    plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test connection string settings from credentials service", "[azureCredentials]") {
  plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");

  auto aws_cred_service = plan->addController("AzureCredentialsService", "AzureCredentialsService");
  plan->setProperty(aws_cred_service, "Connection String", CONNECTION_STRING);
  plan->setProperty(put_azure_blob_storage, "Azure Credentials Service", "AzureCredentialsService");
  test_controller.runSession(plan, true);

  REQUIRE(mock_blob_storage_ptr->getConnectionString() == CONNECTION_STRING);
}

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test Azure blob upload", "[azureBlobStorageUpload]") {
  plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  plan->setProperty(update_attribute, "test.connectionstring", CONNECTION_STRING, true);
  plan->setProperty(put_azure_blob_storage, "Connection String", "${test.connectionstring}");
  plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:azure.container value:" + CONTAINER_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.blobname value:" + BLOB_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.primaryUri value:" + mock_blob_storage_ptr->PRIMARY_URI));
  REQUIRE(LogTestController::getInstance().contains("key:azure.etag value:" + mock_blob_storage_ptr->ETAG));
  REQUIRE(LogTestController::getInstance().contains("key:azure.length value:" + std::to_string(TEST_DATA.size())));
  REQUIRE(LogTestController::getInstance().contains("key:azure.timestamp value:" + mock_blob_storage_ptr->TEST_TIMESTAMP));
  REQUIRE(mock_blob_storage_ptr->input_data == TEST_DATA);
  REQUIRE(mock_blob_storage_ptr->getContainerCreated() == false);
  REQUIRE(mock_blob_storage_ptr->getConnectionString() == CONNECTION_STRING);
  REQUIRE(mock_blob_storage_ptr->getContainerName() == CONTAINER_NAME);
}

TEST_CASE_METHOD(PutAzureBlobStorageTestsFixture, "Test Azure blob upload with container creation", "[azureBlobStorageUpload]") {
  plan->setProperty(update_attribute, "test.container", CONTAINER_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Container Name", "${test.container}");
  plan->setProperty(update_attribute, "test.connectionstring", CONNECTION_STRING, true);
  plan->setProperty(put_azure_blob_storage, "Connection String", "${test.connectionstring}");
  plan->setProperty(update_attribute, "test.blob", BLOB_NAME, true);
  plan->setProperty(put_azure_blob_storage, "Blob", "${test.blob}");
  plan->setProperty(put_azure_blob_storage, "Create Container", "true");
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:azure.container value:" + CONTAINER_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.blobname value:" + BLOB_NAME));
  REQUIRE(LogTestController::getInstance().contains("key:azure.primaryUri value:" + mock_blob_storage_ptr->PRIMARY_URI));
  REQUIRE(LogTestController::getInstance().contains("key:azure.etag value:" + mock_blob_storage_ptr->ETAG));
  REQUIRE(LogTestController::getInstance().contains("key:azure.length value:" + std::to_string(TEST_DATA.size())));
  REQUIRE(LogTestController::getInstance().contains("key:azure.timestamp value:" + mock_blob_storage_ptr->TEST_TIMESTAMP));
  REQUIRE(mock_blob_storage_ptr->input_data == TEST_DATA);
  REQUIRE(mock_blob_storage_ptr->getContainerCreated() == true);
  REQUIRE(mock_blob_storage_ptr->getConnectionString() == CONNECTION_STRING);
  REQUIRE(mock_blob_storage_ptr->getContainerName() == CONTAINER_NAME);
}

}  // namespace
