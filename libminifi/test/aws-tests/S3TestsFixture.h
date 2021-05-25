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
#include <memory>
#include <utility>
#include <string>

#include "core/Processor.h"
#include "../TestBase.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "utils/file/FileUtils.h"
#include "MockS3RequestSender.h"
#include "utils/TestUtils.h"
#include "AWSCredentialsProvider.h"

using org::apache::nifi::minifi::utils::createTempDir;

template<typename T>
class S3TestsFixture {
 public:
  const std::string INPUT_FILENAME = "input_data.log";
  const std::string INPUT_DATA = "input_data";
  const std::string S3_BUCKET = "testBucket";

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
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<T>();
    LogTestController::getInstance().setDebug<minifi::aws::AWSCredentialsProvider>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_request_sender_ptr = new MockS3RequestSender();
    std::unique_ptr<minifi::aws::s3::S3RequestSender> mock_s3_request_sender(mock_s3_request_sender_ptr);
    s3_processor = std::shared_ptr<T>(new T("S3Processor", utils::Identifier(), std::move(mock_s3_request_sender)));
    aws_credentials_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  }

  void setAccessKeyCredentialsInController() {
    plan->setProperty(aws_credentials_service, "Access Key", "key");
    plan->setProperty(aws_credentials_service, "Secret Key", "secret");
  }

  template<typename Component>
  void setCredentialFile(const Component &component) {
    auto temp_path = createTempDir(&test_controller);
    REQUIRE(!temp_path.empty());
    std::string aws_credentials_file(temp_path + utils::file::FileUtils::get_separator() + "aws_creds.conf");
    std::ofstream aws_credentials_file_stream(aws_credentials_file);
    aws_credentials_file_stream << "accessKey=key" << std::endl;
    aws_credentials_file_stream << "secretKey=secret" << std::endl;
    aws_credentials_file_stream.close();
    plan->setProperty(component, "Credentials File", aws_credentials_file);
  }

  template<typename Component>
  void setUseDefaultCredentialsChain(const Component &component) {
    #ifdef WIN32
    _putenv_s("AWS_ACCESS_KEY_ID", "key");
    _putenv_s("AWS_SECRET_ACCESS_KEY", "secret");
    #else
    setenv("AWS_ACCESS_KEY_ID", "key", 1);
    setenv("AWS_SECRET_ACCESS_KEY", "secret", 1);
    #endif
    plan->setProperty(component, "Use Default Credentials", "true");
  }

  void setCredentialsService() {
    plan->setProperty(s3_processor, "AWS Credentials Provider service", "AWSCredentialsService");
  }

  virtual void setAccesKeyCredentialsInProcessor() = 0;
  virtual void setBucket() = 0;
  virtual void setProxy() = 0;

  void setRequiredProperties() {
    setAccesKeyCredentialsInProcessor();
    setBucket();
  }

  void checkProxySettings() {
    REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyHost == "host");
    REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyPort == 1234);
    REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyUserName == "username");
    REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyPassword == "password");
  }

  virtual ~S3TestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  MockS3RequestSender* mock_s3_request_sender_ptr;
  std::shared_ptr<core::Processor> s3_processor;
  std::shared_ptr<core::Processor> update_attribute;
  std::shared_ptr<core::controller::ControllerServiceNode> aws_credentials_service;
};

template<typename T>
class FlowProcessorS3TestsFixture : public S3TestsFixture<T> {
 public:
  const std::string INPUT_FILENAME = "input_data.log";
  const std::string INPUT_DATA = "input_data";

  FlowProcessorS3TestsFixture() {
    LogTestController::getInstance().setTrace<processors::GetFile>();
    LogTestController::getInstance().setDebug<processors::UpdateAttribute>();

    auto input_dir = createTempDir(&this->test_controller);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + INPUT_FILENAME);
    input_file_stream << INPUT_DATA;
    input_file_stream.close();
    auto get_file = this->plan->addProcessor("GetFile", "GetFile");
    this->plan->setProperty(get_file, processors::GetFile::Directory.getName(), input_dir);
    this->plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "false");
    update_attribute = this->plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "d"),
      true);
    this->plan->addProcessor(
      this->s3_processor,
      "S3Processor",
      core::Relationship("success", "d"),
      true);
    auto log_attribute = this->plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
    this->plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  }

  void setAccesKeyCredentialsInProcessor() override {
    this->plan->setProperty(update_attribute, "s3.accessKey", "key", true);
    this->plan->setProperty(this->s3_processor, "Access Key", "${s3.accessKey}");
    this->plan->setProperty(update_attribute, "s3.secretKey", "secret", true);
    this->plan->setProperty(this->s3_processor, "Secret Key", "${s3.secretKey}");
  }

  void setBucket() override {
    this->plan->setProperty(update_attribute, "test.bucket", this->S3_BUCKET, true);
    this->plan->setProperty(this->s3_processor, "Bucket", "${test.bucket}");
  }

  void setProxy() override {
    this->plan->setProperty(update_attribute, "test.proxyHost", "host", true);
    this->plan->setProperty(this->s3_processor, "Proxy Host", "${test.proxyHost}");
    this->plan->setProperty(update_attribute, "test.proxyPort", "1234", true);
    this->plan->setProperty(this->s3_processor, "Proxy Port", "${test.proxyPort}");
    this->plan->setProperty(update_attribute, "test.proxyUsername", "username", true);
    this->plan->setProperty(this->s3_processor, "Proxy Username", "${test.proxyUsername}");
    this->plan->setProperty(update_attribute, "test.proxyPassword", "password", true);
    this->plan->setProperty(this->s3_processor, "Proxy Password", "${test.proxyPassword}");
  }

 protected:
  std::shared_ptr<core::Processor> update_attribute;
};

template<typename T>
class FlowProducerS3TestsFixture : public S3TestsFixture<T> {
 public:
  FlowProducerS3TestsFixture() {
    this->plan->addProcessor(
      this->s3_processor,
      "S3Processor");
    auto log_attribute = this->plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
    this->plan->setProperty(log_attribute, processors::LogAttribute::FlowFilesToLog.getName(), "0");
  }

  void setAccesKeyCredentialsInProcessor() override {
    this->plan->setProperty(this->s3_processor, "Access Key", "key");
    this->plan->setProperty(this->s3_processor, "Secret Key", "secret");
  }

  void setBucket() override {
    this->plan->setProperty(this->s3_processor, "Bucket", this->S3_BUCKET);
  }

  void setProxy() override {
    this->plan->setProperty(this->s3_processor, "Proxy Host", "host");
    this->plan->setProperty(this->s3_processor, "Proxy Port", "1234");
    this->plan->setProperty(this->s3_processor, "Proxy Username", "username");
    this->plan->setProperty(this->s3_processor, "Proxy Password", "password");
  }
};
