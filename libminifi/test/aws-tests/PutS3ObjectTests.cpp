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
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "s3/AbstractS3Wrapper.h"
#include "utils/file/FileUtils.h"

class MockS3Wrapper : public minifi::aws::processors::AbstractS3Wrapper {
public:
  void setCredentials(const Aws::Auth::AWSCredentials& cred) override {
    credentials = cred;
  }
  void setRegion(const Aws::String& region) override {}
  void setTimeout(uint64_t timeout) override {}
  void setEndpointOverrideUrl(const Aws::String& url) override {}
  utils::optional<minifi::aws::processors::PutObjectResult> putObject(const minifi::aws::processors::PutS3ObjectOptions& options, std::shared_ptr<Aws::IOStream> data_stream) override {
    return minifi::aws::processors::PutObjectResult{};
  }

  Aws::Auth::AWSCredentials credentials;
};

class PutS3ObjectTestsFixture {
public:
  PutS3ObjectTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<processors::GetFile>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::aws::processors::PutS3Object>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_wrapper_raw = new MockS3Wrapper();
    std::unique_ptr<minifi::aws::processors::AbstractS3Wrapper> mock_s3_wrapper(mock_s3_wrapper_raw);
    put_s3_object = std::make_shared<minifi::aws::processors::PutS3Object>("PutS3Object", utils::Identifier(), std::move(mock_s3_wrapper));

    char input_dir_mask[] = "/tmp/gt.XXXXXX";
    auto input_dir = test_controller.createTempDirectory(input_dir_mask);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + "input_data.log");
    input_file_stream << "input_data" << std::endl;
    input_file_stream.close();
    auto get_file = plan->addProcessor("GetFile", "GetFile");
    plan->setProperty(get_file, processors::GetFile::Directory.getName(), input_dir);
    plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "false");
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

  void checkDefaultAttributes() {
    REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
    REQUIRE(LogTestController::getInstance().contains("key:s3.key value:input_data.log"));
    REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/octet-stream"));
  }

  std::string createTempFile(const std::string& filename) {
    char temp_dir[] = "/tmp/gt.XXXXXX";
    auto temp_path = test_controller.createTempDirectory(temp_dir);
    REQUIRE(!temp_path.empty());
    std::string temp_file(temp_path + utils::file::FileUtils::get_separator() + filename);
    return temp_file;
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
  checkDefaultAttributes();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test credentials file setting", "[awsCredentials]") {
  char in_dir[] = "/tmp/gt.XXXXXX";
  auto temp_path = test_controller.createTempDirectory(in_dir);
  REQUIRE(!temp_path.empty());
  std::string aws_credentials_file(temp_path + utils::file::FileUtils::get_separator() + "aws_creds.conf");
  std::ofstream aws_credentials_file_stream(aws_credentials_file);
  aws_credentials_file_stream << "accessKey=key1" << std::endl;
  aws_credentials_file_stream << "secretKey=secret1" << std::endl;
  aws_credentials_file_stream.close();
  plan->setProperty(put_s3_object, "Credentials File", aws_credentials_file);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSAccessKeyId() == "key1");
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSSecretKey() == "secret1");
  checkDefaultAttributes();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test credentials setting from AWS Credential service", "[awsCredentials]") {
  auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  plan->setProperty(aws_cred_service, "Access Key", "key2");
  plan->setProperty(aws_cred_service, "Secret Key", "secret2");
  plan->setProperty(put_s3_object, "AWS Credentials Provider service", "AWSCredentialsService");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSAccessKeyId() == "key2");
  REQUIRE(mock_s3_wrapper_raw->credentials.GetAWSSecretKey() == "secret2");
  checkDefaultAttributes();
}
