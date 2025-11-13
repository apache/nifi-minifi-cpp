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
#include <array>

#include "S3TestsFixture.h"
#include "processors/DeleteS3Object.h"
#include "unit/TestUtils.h"

namespace {

using DeleteS3ObjectTestsFixture = FlowProcessorS3TestsFixture<minifi::aws::processors::DeleteS3Object>;
using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;

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
  REQUIRE(mock_s3_request_sender_ptr->getCredentials().GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_request_sender_ptr->getCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test credentials not set") {
  }

  SECTION("Test no bucket is set") {
    setAccesKeyCredentialsInProcessor();
  }

  SECTION("Test no object key is set") {
    setRequiredProperties();
    plan->setDynamicProperty(update_attribute, "filename", "");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Non blank validator tests") {
  setRequiredProperties();
  CHECK_FALSE(plan->setProperty(s3_processor, "Region", ""));
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  SECTION("Use proxy configuration service") {
    setProxy(true);
  }
  SECTION("Use processor properties") {
    setProxy(false);
  }
  test_controller.runSession(plan, true);
  checkProxySettings();
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test success case with default values", "[awsS3DeleteSuccess]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.GetBucket() == "testBucket");
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.GetKey() == INPUT_FILENAME);
  REQUIRE(!mock_s3_request_sender_ptr->delete_object_request.VersionIdHasBeenSet());
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "Successfully deleted S3 object"));
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test version setting", "[awsS3DeleteWithVersion]") {
  setRequiredProperties();
  plan->setDynamicProperty(update_attribute, "s3.version", "v1");
  plan->setProperty(s3_processor, "Version", "${s3.version}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.GetVersionId() == "v1");
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.VersionIdHasBeenSet());
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "Successfully deleted S3 object"));
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test optional client configuration values", "[awsS3DeleteOptionalClientConfig]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Region", minifi::aws::processors::region::US_EAST_1);
  plan->setProperty(s3_processor, "Communications Timeout", "10 Sec");
  plan->setDynamicProperty(update_attribute, "test.endpoint", "http://localhost:1234");
  plan->setProperty(s3_processor, "Endpoint Override URL", "${test.endpoint}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().region == minifi::aws::processors::region::US_EAST_1);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().connectTimeoutMs == 10000);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test failure case", "[awsS3DeleteFailure]") {
  auto log_failure = plan->addProcessor(
      "LogAttribute",
      "LogFailure",
      core::Relationship("failure", "d"));
  plan->addConnection(s3_processor, core::Relationship("failure", "d"), log_failure);
  setRequiredProperties();
  plan->setProperty(s3_processor, "Version", "v1");
  log_failure->setAutoTerminatedRelationships(std::array{core::Relationship("success", "d")});
  mock_s3_request_sender_ptr->setDeleteObjectResult(false);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.GetBucket() == "testBucket");
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.GetKey() == INPUT_FILENAME);
  REQUIRE(mock_s3_request_sender_ptr->delete_object_request.GetVersionId() == "v1");
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "Failed to delete S3 object"));
}

}  // namespace
