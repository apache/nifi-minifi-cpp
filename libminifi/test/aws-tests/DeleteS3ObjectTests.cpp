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

#include "S3TestsFixture.h"
#include "processors/DeleteS3Object.h"

using DeleteS3ObjectTestsFixture = S3TestsFixture<minifi::aws::processors::DeleteS3Object>;

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test AWS credential setting", "[awsCredentials]") {
  setBucket();

  SECTION("Test property credentials") {
    setAccesKeyCredentialsInProcessor();
  }

  SECTION("Test credentials setting from AWS Credentials service") {
    setAccessKeyCredentialsInController();
    setCredentialsService();
  }

  SECTION("Test credentials file setting") {
    setCredentialFile(s3_processor);
  }

  SECTION("Test credentials file setting from AWS Credentials service") {
    setCredentialFile(aws_credentials_service);
    setCredentialsService();
  }

  SECTION("Test credentials setting using default credential chain") {
    setUseDefaultCredentialsChain(s3_processor);
  }

  SECTION("Test credentials setting from AWS Credentials service using default credential chain") {
    setUseDefaultCredentialsChain(aws_credentials_service);
    setCredentialsService();
  }

  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test no bucket is set") {
    setAccesKeyCredentialsInProcessor();
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test success case", "[awsS3DeleteSuccess]") {
  setRequiredProperties();
  plan->setProperty(update_attribute, "s3.version", "v1", true);
  plan->setProperty(s3_processor, "Version", "${s3.version}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->bucket_name == "testBucket");
  REQUIRE(mock_s3_wrapper_ptr->object_key == INPUT_FILENAME);
  REQUIRE(mock_s3_wrapper_ptr->version == "v1");
  REQUIRE(mock_s3_wrapper_ptr->version_has_been_set);
  REQUIRE(LogTestController::getInstance().contains("Successfully deleted S3 object"));
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test empty version", "[awsS3DeleteNoVersion]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(!mock_s3_wrapper_ptr->version_has_been_set);
  REQUIRE(LogTestController::getInstance().contains("Successfully deleted S3 object"));
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test failure case", "[awsS3DeleteFailure]") {
  auto log_failure = plan->addProcessor(
      "LogAttribute",
      "LogFailure",
      core::Relationship("failure", "d"));
  plan->addConnection(s3_processor, core::Relationship("failure", "d"), log_failure);
  setRequiredProperties();
  plan->setProperty(s3_processor, "Version", "v1");
  log_failure->setAutoTerminatedRelationships({{core::Relationship("success", "d")}});
  mock_s3_wrapper_ptr->delete_object_result = false;
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->bucket_name == "testBucket");
  REQUIRE(mock_s3_wrapper_ptr->object_key == INPUT_FILENAME);
  REQUIRE(mock_s3_wrapper_ptr->version == "v1");
  REQUIRE(LogTestController::getInstance().contains("Failed to delete S3 object"));
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  setProxy();
  test_controller.runSession(plan, true);
  checkProxySettings();
}
