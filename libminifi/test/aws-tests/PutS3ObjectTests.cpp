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
#include "s3/S3WrapperBase.h"
#include "utils/file/FileUtils.h"

#include <iostream>
#include <stdlib.h>
#include <map>

const std::string S3_VERSION = "1.2.3";
const std::string S3_ETAG = "tag-123";
const std::string S3_EXPIRATION = "2020-02-20";
const std::string S3_SSEALGORITHM = "aws:kms";

class MockS3Wrapper : public minifi::aws::s3::S3WrapperBase {
public:
  Aws::Auth::AWSCredentials getCredentials() const {
    return credentials_;
  }

  Aws::Client::ClientConfiguration getClientConfig() const {
    return client_config_;
  }

  utils::optional<minifi::aws::s3::PutObjectResult> putObject(const Aws::S3::Model::PutObjectRequest& request) override {
    std::istreambuf_iterator<char> eos;
    put_s3_data = std::string(std::istreambuf_iterator<char>(*request.GetBody()), eos);
    bucket_name = request.GetBucket();
    object_key = request.GetKey();
    storage_class = request.GetStorageClass();
    server_side_encryption = request.GetServerSideEncryption();
    metadata_map = request.GetMetadata();
    content_type = request.GetContentType();

    put_s3_result.version = S3_VERSION;
    put_s3_result.etag = S3_ETAG;
    put_s3_result.expiration = S3_EXPIRATION;
    put_s3_result.ssealgorithm = S3_SSEALGORITHM;
    return put_s3_result;
  }

  std::string bucket_name;
  std::string object_key;
  Aws::S3::Model::StorageClass storage_class;
  Aws::S3::Model::ServerSideEncryption server_side_encryption;
  minifi::aws::s3::PutObjectResult put_s3_result;
  std::string put_s3_data;
  std::map<std::string, std::string> metadata_map;
  std::string content_type;
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

    // Disable AWS get default region requests
    char aws_env[]="AWS_EC2_METADATA_DISABLED=true";
    putenv(aws_env);

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_wrapper_raw = new MockS3Wrapper();
    std::unique_ptr<minifi::aws::s3::S3WrapperBase> mock_s3_wrapper(mock_s3_wrapper_raw);
    put_s3_object = std::make_shared<minifi::aws::processors::PutS3Object>("PutS3Object", utils::Identifier(), std::move(mock_s3_wrapper));

    char input_dir_mask[] = "/tmp/gt.XXXXXX";
    auto input_dir = test_controller.createTempDirectory(input_dir_mask);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + "input_data.log");
    input_file_stream << "input_data";
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

  void setBasicCredentials() {
    plan->setProperty(put_s3_object, "Access Key", "key");
    plan->setProperty(put_s3_object, "Secret Key", "secret");
  }

  void checkPutObjectResults() {
    REQUIRE(LogTestController::getInstance().contains("key:s3.version value:" + S3_VERSION));
    REQUIRE(LogTestController::getInstance().contains("key:s3.etag value:" + S3_ETAG));
    REQUIRE(LogTestController::getInstance().contains("key:s3.expiration value:" + S3_EXPIRATION));
    REQUIRE(LogTestController::getInstance().contains("key:s3.sseAlgorithm value:" + S3_SSEALGORITHM));
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
  REQUIRE(mock_s3_wrapper_raw->getCredentials().GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_raw->getCredentials().GetAWSSecretKey() == "secret");
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
  REQUIRE(mock_s3_wrapper_raw->getCredentials().GetAWSAccessKeyId() == "key1");
  REQUIRE(mock_s3_wrapper_raw->getCredentials().GetAWSSecretKey() == "secret1");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test credentials setting from AWS Credential service", "[awsCredentials]") {
  auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
  plan->setProperty(aws_cred_service, "Access Key", "key2");
  plan->setProperty(aws_cred_service, "Secret Key", "secret2");
  plan->setProperty(put_s3_object, "AWS Credentials Provider service", "AWSCredentialsService");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->getCredentials().GetAWSAccessKeyId() == "key2");
  REQUIRE(mock_s3_wrapper_raw->getCredentials().GetAWSSecretKey() == "secret2");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test no credentials set", "[awsCredentials]") {
  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration", "[awsClientConfig]") {
  setBasicCredentials();
  test_controller.runSession(plan, true);
  checkPutObjectResults();
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:input_data.log"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/octet-stream"));
  REQUIRE(mock_s3_wrapper_raw->content_type == "application/octet-stream");
  REQUIRE(mock_s3_wrapper_raw->storage_class == Aws::S3::Model::StorageClass::STANDARD);
  REQUIRE(mock_s3_wrapper_raw->server_side_encryption == Aws::S3::Model::ServerSideEncryption::NOT_SET);
  REQUIRE(mock_s3_wrapper_raw->getClientConfig().region == minifi::aws::processors::region::US_WEST_2);
  REQUIRE(mock_s3_wrapper_raw->getClientConfig().connectTimeoutMs == 30000);
  REQUIRE(mock_s3_wrapper_raw->getClientConfig().endpointOverride.empty());
  REQUIRE(mock_s3_wrapper_raw->put_s3_data == "input_data");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Set non-default client configuration", "[awsClientConfig]") {
  setBasicCredentials();
  plan->setProperty(put_s3_object, "Object Key", "custom_key");
  plan->setProperty(put_s3_object, "Content Type", "application/tar");
  plan->setProperty(put_s3_object, "Storage Class", minifi::aws::processors::storage_class::REDUCED_REDUNDANCY);
  plan->setProperty(put_s3_object, "Region", minifi::aws::processors::region::US_EAST_1);
  plan->setProperty(put_s3_object, "Communications Timeout", "10 Sec");
  plan->setProperty(put_s3_object, "Endpoint Override URL", "http://localhost:1234");
  plan->setProperty(put_s3_object, "Server Side Encryption", minifi::aws::processors::server_side_encryption::AES256);
  test_controller.runSession(plan, true);
  checkPutObjectResults();
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:custom_key"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/tar"));
  REQUIRE(mock_s3_wrapper_raw->content_type == "application/tar");
  REQUIRE(mock_s3_wrapper_raw->storage_class == Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY);
  REQUIRE(mock_s3_wrapper_raw->server_side_encryption == Aws::S3::Model::ServerSideEncryption::AES256);
  REQUIRE(mock_s3_wrapper_raw->getClientConfig().region == minifi::aws::processors::region::US_EAST_1);
  REQUIRE(mock_s3_wrapper_raw->getClientConfig().connectTimeoutMs == 10000);
  REQUIRE(mock_s3_wrapper_raw->getClientConfig().endpointOverride == "http://localhost:1234");
  REQUIRE(mock_s3_wrapper_raw->put_s3_data == "input_data");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test single user metadata", "[awsMetaData]") {
  setBasicCredentials();
  plan->setProperty(put_s3_object, "meta_key", "meta_value", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->metadata_map.at("meta_key") == "meta_value");
  REQUIRE(LogTestController::getInstance().contains("key:s3.usermetadata value:meta_key=meta_value"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test multiple user metadata", "[awsMetaData]") {
  setBasicCredentials();
  plan->setProperty(put_s3_object, "meta_key1", "meta_value1", true);
  plan->setProperty(put_s3_object, "meta_key2", "meta_value2", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_raw->metadata_map.at("meta_key1") == "meta_value1");
  REQUIRE(mock_s3_wrapper_raw->metadata_map.at("meta_key2") == "meta_value2");
  REQUIRE(LogTestController::getInstance().contains("key:s3.usermetadata value:meta_key1=meta_value1,meta_key2=meta_value2"));
}
