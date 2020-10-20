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

#include <fstream>

#include "S3TestsFixture.h"
#include "processors/FetchS3Object.h"

class FetchS3ObjectTestsFixture : public S3TestsFixture<minifi::aws::processors::FetchS3Object> {
 public:
  FetchS3ObjectTestsFixture() {
    auto putfile = plan->addProcessor(
      "PutFile",
      "PutFile",
      core::Relationship("success", "d"),
      true);
    char output_dir_mask[] = "/tmp/gt.XXXXXX";
    output_dir = test_controller.createTempDirectory(output_dir_mask);
    plan->setProperty(putfile, "Directory", output_dir);
  }

  std::string output_dir;
};

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test AWS credential setting", "[awsCredentials]") {
  setBucket();

  SECTION("Test property credentials") {
    setBasicCredentials();
  }

  SECTION("Test credentials file setting") {
    setCredentialFile();
  }

  SECTION("Test credentials setting from AWS Credential service") {
    setCredentialsService();
  }

  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test credentials not set") {
  }

  SECTION("Test no bucket is set") {
    setBasicCredentials();
  }

  SECTION("Test region is empty") {
    setRequiredProperties();
    plan->setProperty(s3_processor, "Region", "");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  setProxy();
  test_controller.runSession(plan, true);
  checkProxySettings();
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test default properties", "[awsS3Config]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:" + S3_BUCKET));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:" + INPUT_FILENAME));
  REQUIRE(LogTestController::getInstance().contains("key:path value:\n"));
  REQUIRE(LogTestController::getInstance().contains("key:absolute.path value:" + INPUT_FILENAME));
  REQUIRE(LogTestController::getInstance().contains("key:mime.type value:" + S3_CONTENT_TYPE));
  REQUIRE(LogTestController::getInstance().contains("key:s3.etag value:" + S3_ETAG_UNQUOTED));
  REQUIRE(LogTestController::getInstance().contains("key:s3.expirationTime value:" + S3_EXPIRATION_DATE));
  REQUIRE(LogTestController::getInstance().contains("key:s3.expirationTimeRuleId value:" + S3_EXPIRATION_TIME_RULE_ID));
  REQUIRE(LogTestController::getInstance().contains("key:s3.sseAlgorithm value:" + S3_SSEALGORITHM_STR));
  REQUIRE(LogTestController::getInstance().contains("key:s3.version value:" + S3_VERSION));
  std::ifstream s3_file(output_dir + "/" + INPUT_FILENAME);
  std::string output((std::istreambuf_iterator<char>(s3_file)), std::istreambuf_iterator<char>());
  REQUIRE(output == S3_CONTENT);
  REQUIRE(mock_s3_wrapper_ptr->get_object_request.GetVersionId().empty());
  REQUIRE(!mock_s3_wrapper_ptr->get_object_request.VersionIdHasBeenSet());
  REQUIRE(mock_s3_wrapper_ptr->get_object_request.GetRequestPayer() == Aws::S3::Model::RequestPayer::NOT_SET);
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test empty optional S3 results", "[awsS3Config]") {
  setRequiredProperties();
  mock_s3_wrapper_ptr->returnEmptyS3Result();
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:" + S3_BUCKET));
  REQUIRE(LogTestController::getInstance().contains("key:filename value:" + INPUT_FILENAME));
  REQUIRE(LogTestController::getInstance().contains("key:path value:\n"));
  REQUIRE(LogTestController::getInstance().contains("key:absolute.path value:" + INPUT_FILENAME));
  REQUIRE(!LogTestController::getInstance().contains("key:mime.type", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE(!LogTestController::getInstance().contains("key:s3.etag", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE(!LogTestController::getInstance().contains("key:s3.expirationTime", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE(!LogTestController::getInstance().contains("key:s3.expirationTimeRuleId", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE(!LogTestController::getInstance().contains("key:s3.sseAlgorithm", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  REQUIRE(!LogTestController::getInstance().contains("key:s3.version", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  std::ifstream s3_file(output_dir + "/" + INPUT_FILENAME);
  std::string output((std::istreambuf_iterator<char>(s3_file)), std::istreambuf_iterator<char>());
  REQUIRE(output.empty());
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test subdirectories on AWS", "[awsS3Config]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Object Key", "dir1/dir2/logs.txt");
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:filename value:logs.txt"));
  REQUIRE(LogTestController::getInstance().contains("key:path value:dir1/dir2"));
  REQUIRE(LogTestController::getInstance().contains("key:absolute.path value:dir1/dir2/logs.txt"));
  std::ifstream s3_file(output_dir + "/" + INPUT_FILENAME);
  std::string output((std::istreambuf_iterator<char>(s3_file)), std::istreambuf_iterator<char>());
  REQUIRE(output.empty());
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test optional values are set in request", "[awsS3Config]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Version", S3_VERSION);
  plan->setProperty(s3_processor, "Requester Pays", "true");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->get_object_request.GetVersionId() == S3_VERSION);
  REQUIRE(mock_s3_wrapper_ptr->get_object_request.GetRequestPayer() == Aws::S3::Model::RequestPayer::requester);
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test non-default client configuration values", "[awsS3Config]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Region", minifi::aws::processors::region::US_EAST_1);
  plan->setProperty(s3_processor, "Communications Timeout", "10 Sec");
  plan->setProperty(update_attribute, "test.endpoint", "http://localhost:1234", true);
  plan->setProperty(s3_processor, "Endpoint Override URL", "${test.endpoint}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().region == minifi::aws::processors::region::US_EAST_1);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().connectTimeoutMs == 10000);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
}
