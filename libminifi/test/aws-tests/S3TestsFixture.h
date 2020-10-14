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

#include <stdlib.h>
#include <iostream>

#include "core/Processor.h"
#include "../TestBase.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "utils/file/FileUtils.h"
#include "MockS3Wrapper.h"

template<typename T>
class S3TestsFixture {
 public:
  const std::string INPUT_FILENAME = "input_data.log";
  const std::string INPUT_DATA = "input_data";

  S3TestsFixture() {
    // Disable retrieving AWS metadata for tests
    #ifdef WIN32
    _putenv_s("AWS_EC2_METADATA_DISABLED", "true");
    #else
    setenv("AWS_EC2_METADATA_DISABLED", "true", 1);
    #endif

    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<processors::GetFile>();
    LogTestController::getInstance().setDebug<processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<T>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_wrapper_ptr = new MockS3Wrapper();
    std::unique_ptr<minifi::aws::s3::S3WrapperBase> mock_s3_wrapper(mock_s3_wrapper_ptr);
    s3_processor = std::make_shared<T>("S3Processor", utils::Identifier(), std::move(mock_s3_wrapper));

    char input_dir_mask[] = "/tmp/gt.XXXXXX";
    auto input_dir = test_controller.createTempDirectory(input_dir_mask);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + INPUT_FILENAME);
    input_file_stream << INPUT_DATA;
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
      s3_processor,
      "PutS3Object",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
  }

  void setBasicCredentials() {
    plan->setProperty(s3_processor, "Access Key", "key");
    plan->setProperty(s3_processor, "Secret Key", "secret");
  }

  void setBucket() {
    plan->setProperty(update_attribute, "test.bucket", "testBucket", true);
    plan->setProperty(s3_processor, "Bucket", "${test.bucket}");
  }

  void setRequiredProperties() {
    setBasicCredentials();
    setBucket();
  }

  std::string createTempFile(const std::string& filename) {
    char temp_dir[] = "/tmp/gt.XXXXXX";
    auto temp_path = test_controller.createTempDirectory(temp_dir);
    REQUIRE(!temp_path.empty());
    std::string temp_file(temp_path + utils::file::FileUtils::get_separator() + filename);
    return temp_file;
  }

  virtual ~S3TestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  MockS3Wrapper* mock_s3_wrapper_ptr;
  std::shared_ptr<core::Processor> s3_processor;
  std::shared_ptr<core::Processor> get_file;
  std::shared_ptr<core::Processor> update_attribute;
};
