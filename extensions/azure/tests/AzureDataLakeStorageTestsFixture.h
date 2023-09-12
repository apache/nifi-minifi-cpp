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

#pragma once

#include <array>
#include <utility>
#include <vector>
#include <memory>
#include <string>

#include "MockDataLakeStorageClient.h"
#include "../TestBase.h"
#include "../Catch.h"
#include "utils/TestUtils.h"
#include "utils/IntegrationTestUtils.h"
#include "core/Processor.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "utils/file/FileUtils.h"
#include "controllerservices/AzureStorageCredentialsService.h"

const std::string FILESYSTEM_NAME = "testfilesystem";
const std::string DIRECTORY_NAME = "testdir";
const std::string FILE_NAME = "testfile.txt";
const std::string CONNECTION_STRING = "test-connectionstring";
const std::string TEST_DATA = "data123";
const std::string GETFILE_FILE_NAME = "input_data.log";

template<typename AzureDataLakeStorageProcessor>
class AzureDataLakeStorageTestsFixture {
 public:
  AzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<minifi::processors::GetFile>();
    LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
    LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<AzureDataLakeStorageProcessor>();

    // Build MiNiFi processing graph
    plan_ = test_controller_.createPlan();
    auto mock_data_lake_storage_client = std::make_unique<MockDataLakeStorageClient>();
    mock_data_lake_storage_client_ptr_ = mock_data_lake_storage_client.get();
    azure_data_lake_storage_ = std::shared_ptr<AzureDataLakeStorageProcessor>(
      new AzureDataLakeStorageProcessor("AzureDataLakeStorageProcessor", utils::Identifier(), std::move(mock_data_lake_storage_client)));
    auto input_dir = test_controller_.createTempDirectory();
    utils::putFileToDir(input_dir, GETFILE_FILE_NAME, TEST_DATA);

    get_file_ = plan_->addProcessor("GetFile", "GetFile");
    plan_->setProperty(get_file_, minifi::processors::GetFile::Directory, input_dir.string());
    plan_->setProperty(get_file_, minifi::processors::GetFile::KeepSourceFile, "false");

    update_attribute_ = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", { {"success", "d"} },  true);
    plan_->addProcessor(azure_data_lake_storage_, "AzureDataLakeStorageProcessor", { {"success", "d"}, {"failure", "d"} }, true);
    auto logattribute = plan_->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);

    success_putfile_ = plan_->addProcessor("PutFile", "SuccessPutFile", { {"success", "d"} }, false);
    plan_->addConnection(logattribute, {"success", "d"}, success_putfile_);
    success_putfile_->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
    success_output_dir_ = test_controller_.createTempDirectory();
    plan_->setProperty(success_putfile_, org::apache::nifi::minifi::processors::PutFile::Directory, success_output_dir_.string());

    failure_putfile_ = plan_->addProcessor("PutFile", "FailurePutFile", { {"success", "d"} }, false);
    plan_->addConnection(azure_data_lake_storage_, {"failure", "d"}, failure_putfile_);
    failure_putfile_->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
    failure_output_dir_ = test_controller_.createTempDirectory();
    plan_->setProperty(failure_putfile_, org::apache::nifi::minifi::processors::PutFile::Directory, failure_output_dir_.string());

    azure_storage_cred_service_ = plan_->addController("AzureStorageCredentialsService", "AzureStorageCredentialsService");
    setDefaultProperties();
  }

  std::vector<std::string> getFailedFlowFileContents() {
    return getFileContents(failure_output_dir_);
  }

  std::vector<std::string> getSuccessfulFlowFileContents() {
    return getFileContents(success_output_dir_);
  }

  std::vector<std::string> getFileContents(const std::filesystem::path& dir) {
    std::vector<std::string> file_contents;

    auto lambda = [&file_contents](const std::filesystem::path& path, const std::filesystem::path& filename) -> bool {
      std::ifstream is(path / filename, std::ifstream::binary);
      file_contents.push_back(std::string((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>()));
      return true;
    };

    utils::file::FileUtils::list_dir(dir, lambda, plan_->getLogger(), false);
    return file_contents;
  }

  void setDefaultProperties() {
    plan_->setProperty(azure_data_lake_storage_, AzureDataLakeStorageProcessor::AzureStorageCredentialsService, "AzureStorageCredentialsService");
    plan_->setDynamicProperty(update_attribute_, "test.filesystemname", FILESYSTEM_NAME);
    plan_->setProperty(azure_data_lake_storage_, AzureDataLakeStorageProcessor::FilesystemName, "${test.filesystemname}");
    plan_->setDynamicProperty(update_attribute_, "test.directoryname", DIRECTORY_NAME);
    plan_->setProperty(azure_data_lake_storage_, AzureDataLakeStorageProcessor::DirectoryName, "${test.directoryname}");
    plan_->setProperty(azure_storage_cred_service_, minifi::azure::controllers::AzureStorageCredentialsService::ConnectionString, CONNECTION_STRING);
  }

  virtual ~AzureDataLakeStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockDataLakeStorageClient* mock_data_lake_storage_client_ptr_;
  std::shared_ptr<core::Processor> azure_data_lake_storage_;
  std::shared_ptr<core::Processor> get_file_;
  std::shared_ptr<core::Processor> update_attribute_;
  std::shared_ptr<core::Processor> success_putfile_;
  std::shared_ptr<core::Processor> failure_putfile_;
  std::shared_ptr<core::controller::ControllerServiceNode> azure_storage_cred_service_;
  std::filesystem::path failure_output_dir_;
  std::filesystem::path success_output_dir_;
};
