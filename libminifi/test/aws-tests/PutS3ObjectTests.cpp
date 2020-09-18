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

#include "core/Processor.h"
#include "../TestBase.h"
#include "processors/PutS3Object.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "s3/AbstractS3Wrapper.h"
#include "utils/file/FileUtils.h"

class MockS3Wrapper : public minifi::aws::processors::AbstractS3Wrapper {
public:
  void setCredentials(const Aws::Auth::AWSCredentials& cred) override {
    credentials = cred;
  }

  utils::optional<minifi::aws::processors::PutObjectResult> putObject(const Aws::String& bucketName,
      const Aws::String& objectName,
      const Aws::String& region) override {
    return utils::nullopt;
  }

  Aws::Auth::AWSCredentials credentials;
};

class PutS3ObjectTestsFixture {
public:
  PutS3ObjectTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<processors::GenerateFlowFile>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<org::apache::nifi::minifi::aws::processors::PutS3Object>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_wrapper_raw = new MockS3Wrapper();
    std::unique_ptr<minifi::aws::processors::AbstractS3Wrapper> mock_s3_wrapper(mock_s3_wrapper_raw);
    // mock_s3_wrapper_raw = static_cast<MockS3Wrapper*>(mock_s3_wrapper.get());
    put_s3_object = std::make_shared<org::apache::nifi::minifi::aws::processors::PutS3Object>("PutS3Object", utils::Identifier(), std::move(mock_s3_wrapper));
    plan->addProcessor("GenerateFlowFile", "GenerateFlowFile");
    plan->addProcessor(
      put_s3_object,
      "PutS3Object",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
    plan->setProperty(put_s3_object, "Bucket", "testBucket");
  }

  virtual ~PutS3ObjectTestsFixture() {
    LogTestController::getInstance().reset();
  }

protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  MockS3Wrapper* mock_s3_wrapper_raw;
  std::shared_ptr<core::Processor> put_s3_object;
};

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test basic property credential setting", "[awsCredentials]") {
  plan->setProperty(put_s3_object, "Access Key", "key");
  plan->setProperty(put_s3_object, "Secret Key", "secret");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSSecretKey() == "secret");
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:asd"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test credentials file setting", "[awsCredentials]") {
  char in_dir[] = "/tmp/gt.XXXXXX";
  auto temp_path = test_controller.createTempDirectory(in_dir);
  REQUIRE(!temp_path.empty());
  std::string aws_credentials_file(temp_path + utils::file::FileUtils::get_separator() + "aws_creds.conf");
  std::ofstream aws_credentials_file_stream(aws_credentials_file);
  aws_credentials_file_stream << "accessKey=key" << std::endl;
  aws_credentials_file_stream << "secretKey=secret" << std::endl;
  aws_credentials_file_stream.close();
  plan->setProperty(put_s3_object, "Credentials File", aws_credentials_file);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSSecretKey() == "secret");
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:asd"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test credentials setting from AWS Credential service", "[awsCredentials]") {
  auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  plan->setProperty(aws_cred_service, "Access Key", "key");
  plan->setProperty(aws_cred_service, "Secret Key", "secret");
  plan->setProperty(put_s3_object, "AWS Credentials Provider service", "AWSCredentialsService");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSSecretKey() == "secret");
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:asd"));
}
