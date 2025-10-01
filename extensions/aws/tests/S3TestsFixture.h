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

#include "controllerservices/AWSCredentialsService.h"
#include "core/Processor.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/S3Processor.h"
#include "processors/UpdateAttribute.h"
#include "utils/file/FileUtils.h"
#include "MockS3RequestSender.h"
#include "unit/TestUtils.h"
#include "AWSCredentialsProvider.h"
#include "s3/MultipartUploadStateStorage.h"
#include "s3/S3Wrapper.h"

template<typename T>
class S3TestsFixture {
 public:
  const std::string INPUT_FILENAME = "input_data.log";
  const std::string INPUT_DATA = "input_data";
  const std::string S3_BUCKET = "testBucket";

  S3TestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<minifi::processors::LogAttribute>();
    LogTestController::getInstance().setTrace<T>();
    LogTestController::getInstance().setDebug<minifi::aws::AWSCredentialsProvider>();
    LogTestController::getInstance().setDebug<minifi::aws::s3::MultipartUploadStateStorage>();
    LogTestController::getInstance().setDebug<minifi::aws::s3::S3Wrapper>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    aws_credentials_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  }

  void setAccessKeyCredentialsInController() {
    plan->setProperty(aws_credentials_service, minifi::aws::controllers::AWSCredentialsService::AccessKey, "key");
    plan->setProperty(aws_credentials_service, minifi::aws::controllers::AWSCredentialsService::SecretKey, "secret");
  }

  template<typename Component>
  void setCredentialFile(const Component &component) {
    auto temp_path = test_controller.createTempDirectory();
    REQUIRE(!temp_path.empty());
    auto aws_credentials_file = temp_path / "aws_creds.conf";
    std::ofstream aws_credentials_file_stream(aws_credentials_file);
    aws_credentials_file_stream << "accessKey=key" << std::endl;
    aws_credentials_file_stream << "secretKey=secret" << std::endl;
    aws_credentials_file_stream.close();
    plan->setProperty(component, minifi::aws::processors::S3Processor::CredentialsFile, aws_credentials_file.string());
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
    plan->setProperty(s3_processor, minifi::aws::processors::S3Processor::AWSCredentialsProviderService, "AWSCredentialsService");
  }

  virtual void setAccesKeyCredentialsInProcessor() = 0;
  virtual void setBucket() = 0;
  virtual void setProxy(bool use_controller_service) = 0;

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
  core::Processor* s3_processor;
  core::Processor* update_attribute;
  std::shared_ptr<core::controller::ControllerServiceNode> aws_credentials_service;
};

template<typename T>
class FlowProcessorS3TestsFixture : public S3TestsFixture<T> {
 public:
  const std::string INPUT_FILENAME = "input_data.log";
  const std::string INPUT_DATA = "This data is has a length of 37 bytes";

  FlowProcessorS3TestsFixture() {
    LogTestController::getInstance().setTrace<minifi::processors::GetFile>();
    LogTestController::getInstance().setDebug<minifi::processors::UpdateAttribute>();

    auto mock_s3_request_sender = std::make_unique<MockS3RequestSender>();
    this->mock_s3_request_sender_ptr = mock_s3_request_sender.get();
    auto uuid = utils::IdGenerator::getIdGenerator()->generate();
    auto impl = std::unique_ptr<T>(new T(core::ProcessorMetadata{.uuid = uuid, .name = "S3Processor", .logger = core::logging::LoggerFactory<T>::getLogger(uuid)}, std::move(mock_s3_request_sender)));
    auto s3_processor_unique_ptr = std::make_unique<core::Processor>("S3Processor", uuid, std::move(impl));
    this->s3_processor = s3_processor_unique_ptr.get();

    auto input_dir = this->test_controller.createTempDirectory();
    std::ofstream input_file_stream(input_dir / INPUT_FILENAME);
    input_file_stream << INPUT_DATA;
    input_file_stream.close();
    auto get_file = this->plan->addProcessor("GetFile", "GetFile");
    this->plan->setProperty(get_file, minifi::processors::GetFile::Directory, input_dir.string());
    this->plan->setProperty(get_file, minifi::processors::GetFile::KeepSourceFile, "true");
    update_attribute = this->plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "d"),
      true);
    this->plan->addProcessor(
      std::move(s3_processor_unique_ptr),
      "S3Processor",
      core::Relationship("success", "d"),
      true);
    auto log_attribute = this->plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
    this->plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");
    log_attribute->setAutoTerminatedRelationships(std::array{core::Relationship("success", "d")});
  }

  void setAccesKeyCredentialsInProcessor() override {
    this->plan->setDynamicProperty(update_attribute, "s3.accessKey", "key");
    this->plan->setProperty(this->s3_processor, "Access Key", "${s3.accessKey}");
    this->plan->setDynamicProperty(update_attribute, "s3.secretKey", "secret");
    this->plan->setProperty(this->s3_processor, "Secret Key", "${s3.secretKey}");
  }

  void setBucket() override {
    this->plan->setDynamicProperty(update_attribute, "test.bucket", this->S3_BUCKET);
    this->plan->setProperty(this->s3_processor, "Bucket", "${test.bucket}");
  }

  void setProxy(bool use_controller_service) override {
    if (use_controller_service) {
      auto proxy_configuration_service = this->plan->addController("ProxyConfigurationService", "ProxyConfigurationService");
      this->plan->setProperty(proxy_configuration_service, "Proxy Server Host", "host");
      this->plan->setProperty(proxy_configuration_service, "Proxy Server Port", "1234");
      this->plan->setProperty(proxy_configuration_service, "Proxy User Name", "username");
      this->plan->setProperty(proxy_configuration_service, "Proxy User Password", "password");
      this->plan->setProperty(this->s3_processor, "Proxy Configuration Service", "ProxyConfigurationService");
    } else {
      this->plan->setDynamicProperty(update_attribute, "test.proxyHost", "host");
      this->plan->setProperty(this->s3_processor, "Proxy Host", "${test.proxyHost}");
      this->plan->setDynamicProperty(update_attribute, "test.proxyPort", "1234");
      this->plan->setProperty(this->s3_processor, "Proxy Port", "${test.proxyPort}");
      this->plan->setDynamicProperty(update_attribute, "test.proxyUsername", "username");
      this->plan->setProperty(this->s3_processor, "Proxy Username", "${test.proxyUsername}");
      this->plan->setDynamicProperty(update_attribute, "test.proxyPassword", "password");
      this->plan->setProperty(this->s3_processor, "Proxy Password", "${test.proxyPassword}");
    }
  }

 protected:
  core::Processor* update_attribute;
};

template<typename T>
class FlowProducerS3TestsFixture : public S3TestsFixture<T> {
 public:
  FlowProducerS3TestsFixture() {
    auto mock_s3_request_sender = std::make_unique<MockS3RequestSender>();
    this->mock_s3_request_sender_ptr = mock_s3_request_sender.get();
    auto uuid = utils::IdGenerator::getIdGenerator()->generate();
    auto impl = std::unique_ptr<T>(new T(core::ProcessorMetadata{.uuid = uuid, .name = "S3Processor", .logger = core::logging::LoggerFactory<T>::getLogger(uuid)}, std::move(mock_s3_request_sender)));
    auto s3_processor_unique_ptr = std::make_unique<core::Processor>("S3Processor", uuid, std::move(impl));
    this->s3_processor = s3_processor_unique_ptr.get();

    this->plan->addProcessor(
      std::move(s3_processor_unique_ptr),
      "S3Processor");
    auto log_attribute = this->plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
    this->plan->setProperty(log_attribute, minifi::processors::LogAttribute::FlowFilesToLog, "0");
  }

  void setAccesKeyCredentialsInProcessor() override {
    this->plan->setProperty(this->s3_processor, "Access Key", "key");
    this->plan->setProperty(this->s3_processor, "Secret Key", "secret");
  }

  void setBucket() override {
    this->plan->setProperty(this->s3_processor, "Bucket", this->S3_BUCKET);
  }

  void setProxy(bool use_controller_service) override {
    if (use_controller_service) {
      auto proxy_configuration_service = this->plan->addController("ProxyConfigurationService", "ProxyConfigurationService");
      this->plan->setProperty(proxy_configuration_service, "Proxy Server Host", "host");
      this->plan->setProperty(proxy_configuration_service, "Proxy Server Port", "1234");
      this->plan->setProperty(proxy_configuration_service, "Proxy User Name", "username");
      this->plan->setProperty(proxy_configuration_service, "Proxy User Password", "password");
      this->plan->setProperty(this->s3_processor, "Proxy Configuration Service", "ProxyConfigurationService");
    } else {
      this->plan->setProperty(this->s3_processor, "Proxy Host", "host");
      this->plan->setProperty(this->s3_processor, "Proxy Port", "1234");
      this->plan->setProperty(this->s3_processor, "Proxy Username", "username");
      this->plan->setProperty(this->s3_processor, "Proxy Password", "password");
    }
  }
};
