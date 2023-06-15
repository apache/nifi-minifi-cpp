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
#include <string>
#include <utility>
#include <vector>
#include <memory>

#include "../TestBase.h"
#include "../Catch.h"
#include "core/Processor.h"
#include "processors/GetFile.h"
#include "processors/PutFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "utils/file/FileUtils.h"
#include "MockBlobStorage.h"

const std::string CONTAINER_NAME = "test-container";
const std::string STORAGE_ACCOUNT_NAME = "test-account";
const std::string STORAGE_ACCOUNT_KEY = "test-key";
const std::string SAS_TOKEN = "test-sas-token";
const std::string ENDPOINT_SUFFIX = "test.suffix.com";
const std::string CONNECTION_STRING = "test-connectionstring";
const std::string BLOB_NAME = "test-blob.txt";
const std::string TEST_DATA = "data";
const std::string GET_FILE_NAME = "input_data.log";

template<typename ProcessorType>
class AzureBlobStorageTestsFixture {
 public:
  AzureBlobStorageTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<minifi::processors::GetFile>();
    LogTestController::getInstance().setTrace<minifi::processors::PutFile>();
    LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<ProcessorType>();

    // Build MiNiFi processing graph
    plan_ = test_controller_.createPlan();
    auto mock_blob_storage = std::make_unique<MockBlobStorage>();
    mock_blob_storage_ptr_ = mock_blob_storage.get();
    azure_blob_storage_processor_ = std::shared_ptr<ProcessorType>(
      new ProcessorType("AzureBlobStorageProcessor", utils::Identifier(), std::move(mock_blob_storage)));
    auto input_dir = test_controller_.createTempDirectory();
    std::ofstream input_file_stream(input_dir / GET_FILE_NAME);
    input_file_stream << TEST_DATA;
    input_file_stream.close();
    get_file_processor_ = plan_->addProcessor("GetFile", "GetFile");
    plan_->setProperty(get_file_processor_, minifi::processors::GetFile::Directory, input_dir.string());
    plan_->setProperty(get_file_processor_, minifi::processors::GetFile::KeepSourceFile, "false");
    update_attribute_processor_ = plan_->addProcessor("UpdateAttribute", "UpdateAttribute", { {"success", "d"} },  true);
    plan_->addProcessor(azure_blob_storage_processor_, "AzureBlobStorageProcessor", { {"success", "d"} }, true);
    auto logattribute = plan_->addProcessor("LogAttribute", "LogAttribute", { {"success", "d"} }, true);
    success_putfile_ = plan_->addProcessor("PutFile", "SuccessPutFile", { {"success", "d"} }, false);
    plan_->addConnection(logattribute, {"success", "d"}, success_putfile_);
    success_putfile_->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
    success_output_dir_ = test_controller_.createTempDirectory();
    plan_->setProperty(success_putfile_, org::apache::nifi::minifi::processors::PutFile::Directory, success_output_dir_.string());

    failure_putfile_ = plan_->addProcessor("PutFile", "FailurePutFile", { {"success", "d"} }, false);
    plan_->addConnection(azure_blob_storage_processor_, {"failure", "d"}, failure_putfile_);
    failure_putfile_->setAutoTerminatedRelationships(std::array{core::Relationship{"success", "d"}, core::Relationship{"failure", "d"}});
    failure_output_dir_ = test_controller_.createTempDirectory();
    plan_->setProperty(failure_putfile_, org::apache::nifi::minifi::processors::PutFile::Directory, failure_output_dir_.string());
  }

  void setDefaultCredentials() {
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_name", STORAGE_ACCOUNT_NAME);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Name", "${test.account_name}");
    plan_->setDynamicProperty(update_attribute_processor_, "test.account_key", STORAGE_ACCOUNT_KEY);
    plan_->setProperty(azure_blob_storage_processor_, "Storage Account Key", "${test.account_key}");
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

  std::vector<std::string> getFailedFlowFileContents() {
    return getFileContents(failure_output_dir_);
  }

  std::vector<std::string> getSuccessfulFlowFileContents() {
    return getFileContents(success_output_dir_);
  }

  virtual ~AzureBlobStorageTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_;
  MockBlobStorage* mock_blob_storage_ptr_;
  std::shared_ptr<core::Processor> azure_blob_storage_processor_;
  std::shared_ptr<core::Processor> get_file_processor_;
  std::shared_ptr<core::Processor> update_attribute_processor_;
  std::shared_ptr<core::Processor> success_putfile_;
  std::shared_ptr<core::Processor> failure_putfile_;
  std::filesystem::path failure_output_dir_;
  std::filesystem::path success_output_dir_;
};
