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
#include "utils/IntegrationTestUtils.h"
#include "core/Processor.h"
#include "processors/PutAzureDataLakeStorage.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "storage/DataLakeStorageClient.h"
#include "utils/file/FileUtils.h"
#include "controllerservices/AzureStorageCredentialsService.h"

using namespace std::chrono_literals;

const std::string FILESYSTEM_NAME = "testfilesystem";
const std::string DIRECTORY_NAME = "testdir";
const std::string FILE_NAME = "testfile.txt";
const std::string CONNECTION_STRING = "test-connectionstring";
const std::string TEST_DATA = "data123";
const std::string GETFILE_FILE_NAME = "input_data.log";

class MockDataLakeStorageClient : public minifi::azure::storage::DataLakeStorageClient {
 public:
  const std::string PRIMARY_URI = "http://test-uri/file";

  bool createFile(const minifi::azure::storage::PutAzureDataLakeStorageParameters& /*params*/) override {
    if (file_creation_error_) {
      throw std::runtime_error("error");
    }
    return create_file_;
  }

  std::string uploadFile(const minifi::azure::storage::PutAzureDataLakeStorageParameters& params, const uint8_t* buffer, std::size_t buffer_size) override {
    input_data_ = std::string(buffer, buffer + buffer_size);
    params_ = params;

    if (upload_fails_) {
      throw std::runtime_error("error");
    }

    return RETURNED_PRIMARY_URI;
  }

  void setFileCreation(bool create_file) {
    create_file_ = create_file;
  }

  void setFileCreationError(bool file_creation_error) {
    file_creation_error_ = file_creation_error;
  }

  void setUploadFailure(bool upload_fails) {
    upload_fails_ = upload_fails;
  }

  minifi::azure::storage::PutAzureDataLakeStorageParameters getPassedParams() const {
    return params_;
  }

 private:
  const std::string RETURNED_PRIMARY_URI = "http://test-uri/file?secret-sas";
  bool create_file_ = true;
  bool file_creation_error_ = false;
  bool upload_fails_ = false;
  std::string input_data_;
  minifi::azure::storage::PutAzureDataLakeStorageParameters params_;
};

class PutAzureDataLakeStorageTestsFixture {
 public:
  PutAzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<processors::GetFile>();
    LogTestController::getInstance().setTrace<processors::PutFile>();
    LogTestController::getInstance().setDebug<processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::azure::processors::PutAzureDataLakeStorage>();

    // Build MiNiFi processing graph
    plan_ = test_controller_.createPlan();
    auto mock_data_lake_storage_client = std::make_unique<MockDataLakeStorageClient>();
    mock_data_lake_storage_client_ptr_ = mock_data_lake_storage_client.get();
    put_azure_data_lake_storage_ = std::shared_ptr<minifi::azure::processors::PutAzureDataLakeStorage>(
      new minifi::azure::processors::PutAzureDataLakeStorage("PutAzureDataLakeStorage", utils::Identifier(), std::move(mock_data_lake_storage_client)));
    auto input_dir = test_controller_.createTempDirectory();
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + GETFILE_FILE_NAME);
    input_file_stream << TEST_DATA;
    input_file_stream.close();

    get_file_ = plan_->addProcessor("GetFile", "GetFile");
    plan_->setProperty(get_file_, processors::GetFile::Directory.getName(), input_dir);
    plan_->setProperty(get_file_, processors::GetFile::KeepSourceFile.getName(), "false");

    update_attribute_ = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", { {"success", "d"} },  true);
    plan_->addProcessor(put_azure_data_lake_storage_, "PutAzureDataLakeStorage", { {"success", "d"}, {"failure", "d"} }, true);
    auto logattribute = plan_->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);
    logattribute->setAutoTerminatedRelationships({{"success", "d"}});

    putfile_ = plan_->addProcessor("PutFile", "PutFile", { {"success", "d"} }, false);
    plan_->addConnection(put_azure_data_lake_storage_, {"failure", "d"}, putfile_);
    putfile_->setAutoTerminatedRelationships({{"success", "d"}});
    putfile_->setAutoTerminatedRelationships({{"failure", "d"}});
    output_dir_ = test_controller_.createTempDirectory();
    plan_->setProperty(putfile_, org::apache::nifi::minifi::processors::PutFile::Directory.getName(), output_dir_);

    azure_storage_cred_service_ = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    setDefaultProperties();
  }

  std::vector<std::string> getFailedFlowFileContents() {
    std::vector<std::string> file_contents;

    auto lambda = [&file_contents](const std::string& path, const std::string& filename) -> bool {
      std::ifstream is(path + utils::file::FileUtils::get_separator() + filename, std::ifstream::binary);
      std::string file_content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
      file_contents.push_back(file_content);
      return true;
    };

    utils::file::FileUtils::list_dir(output_dir_, lambda, plan_->getLogger(), false);
    return file_contents;
  }

  void setDefaultProperties() {
    plan_->setProperty(put_azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::AzureStorageCredentialsService.getName(), "AzureStorageCredentialsService");
    plan_->setProperty(update_attribute_, "test.filesystemname", FILESYSTEM_NAME, true);
    plan_->setProperty(put_azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::FilesystemName.getName(), "${test.filesystemname}");
    plan_->setProperty(update_attribute_, "test.directoryname", DIRECTORY_NAME, true);
    plan_->setProperty(put_azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::DirectoryName.getName(), "${test.directoryname}");
    plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), CONNECTION_STRING);
  }

  virtual ~PutAzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockDataLakeStorageClient* mock_data_lake_storage_client_ptr_;
  std::shared_ptr<core::Processor> put_azure_data_lake_storage_;
  std::shared_ptr<core::Processor> get_file_;
  std::shared_ptr<core::Processor> update_attribute_;
  std::shared_ptr<core::Processor> putfile_;
  std::shared_ptr<core::controller::ControllerServiceNode> azure_storage_cred_service_;
  std::string output_dir_;
};

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Azure storage credentials service is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(put_azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::AzureStorageCredentialsService.getName(), "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().size() == 0);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Filesystem name is not set", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(update_attribute_, "test.filesystemname", "", true);
  test_controller_.runSession(plan_, true);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "Filesystem Name '' is invalid or empty!"));
  auto failed_flowfiles = getFailedFlowFileContents();
  REQUIRE(failed_flowfiles.size() == 1);
  REQUIRE(failed_flowfiles[0] == TEST_DATA);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Connection String is empty", "[azureDataLakeStorageParameters]") {
  plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString.getName(), "");
  REQUIRE_THROWS_AS(test_controller_.runSession(plan_, true), minifi::Exception);
  REQUIRE(getFailedFlowFileContents().size() == 0);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Upload to Azure Data Lake Storage with default parameters", "[azureDataLakeStorageUpload]") {
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:" + GETFILE_FILE_NAME));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:" + std::to_string(TEST_DATA.size())));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.primaryUri value:" + mock_data_lake_storage_client_ptr_->PRIMARY_URI));
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedParams();
  REQUIRE(passed_params.connection_string == CONNECTION_STRING);
  REQUIRE(passed_params.file_system_name == FILESYSTEM_NAME);
  REQUIRE(passed_params.directory_name == DIRECTORY_NAME);
  REQUIRE(passed_params.filename == GETFILE_FILE_NAME);
  REQUIRE_FALSE(passed_params.replace_file);
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
  plan_->setProperty(put_azure_data_lake_storage_,
    minifi::azure::processors::PutAzureDataLakeStorage::ConflictResolutionStrategy.getName(), toString(minifi::azure::processors::PutAzureDataLakeStorage::FileExistsResolutionStrategy::IGNORE_REQUEST));
  mock_data_lake_storage_client_ptr_->setFileCreation(false);
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:filename value:" + GETFILE_FILE_NAME));
  REQUIRE(!verifyLogLinePresenceInPollTime(0s, "key:azure"));
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Replace old file on 'replace' resolution strategy if file exists", "[azureDataLakeStorageUpload]") {
  plan_->setProperty(put_azure_data_lake_storage_,
    minifi::azure::processors::PutAzureDataLakeStorage::ConflictResolutionStrategy.getName(), toString(minifi::azure::processors::PutAzureDataLakeStorage::FileExistsResolutionStrategy::REPLACE_FILE));
  mock_data_lake_storage_client_ptr_->setFileCreation(false);
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:" + DIRECTORY_NAME));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.filename value:" + GETFILE_FILE_NAME));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.filesystem value:" + FILESYSTEM_NAME));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.length value:" + std::to_string(TEST_DATA.size())));
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.primaryUri value:" + mock_data_lake_storage_client_ptr_->PRIMARY_URI));
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedParams();
  REQUIRE(passed_params.connection_string == CONNECTION_STRING);
  REQUIRE(passed_params.file_system_name == FILESYSTEM_NAME);
  REQUIRE(passed_params.directory_name == DIRECTORY_NAME);
  REQUIRE(passed_params.filename == GETFILE_FILE_NAME);
  REQUIRE(passed_params.replace_file);
}

TEST_CASE_METHOD(PutAzureDataLakeStorageTestsFixture, "Upload to Azure Data Lake Storage with empty directory is accepted", "[azureDataLakeStorageUpload]") {
  plan_->setProperty(put_azure_data_lake_storage_, minifi::azure::processors::PutAzureDataLakeStorage::DirectoryName.getName(), "");
  test_controller_.runSession(plan_, true);
  REQUIRE(getFailedFlowFileContents().size() == 0);
  using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;
  REQUIRE(verifyLogLinePresenceInPollTime(1s, "key:azure.directory value:\n"));
  auto passed_params = mock_data_lake_storage_client_ptr_->getPassedParams();
  REQUIRE(passed_params.directory_name == "");
}
