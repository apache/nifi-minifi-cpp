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
#include "unit/TestUtils.h"
#include "utils/file/FileUtils.h"

namespace {

using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;
using org::apache::nifi::minifi::utils::file::get_content;

class FetchS3ObjectTestsFixture : public FlowProcessorS3TestsFixture<minifi::aws::processors::FetchS3Object> {
 public:
  FetchS3ObjectTestsFixture() {
    auto putfile = plan->addProcessor(
      "PutFile",
      "PutFile",
      core::Relationship("success", "d"),
      true);
    output_dir = test_controller.createTempDirectory();
    plan->setProperty(putfile, "Directory", output_dir.string());
  }

  std::filesystem::path output_dir;
};

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test AWS credential setting", "[awsCredentials]") {
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

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
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

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Non blank validator tests") {
  setRequiredProperties();
  CHECK_FALSE(plan->setProperty(s3_processor, "Region", ""));
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
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

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test default properties", "[awsS3Config]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:" + S3_BUCKET));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:filename value:" + INPUT_FILENAME));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:path value:\n"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:absolute.path value:" + INPUT_FILENAME));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:mime.type value:" + S3_CONTENT_TYPE));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.etag value:" + S3_ETAG_UNQUOTED));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.expirationTime value:" + S3_EXPIRATION_DATE));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.expirationTimeRuleId value:" + S3_EXPIRATION_TIME_RULE_ID));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.sseAlgorithm value:" + S3_SSEALGORITHM_STR));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.version value:" + S3_VERSION_1));
  REQUIRE(get_content(output_dir / INPUT_FILENAME) == S3_CONTENT);
  REQUIRE(mock_s3_request_sender_ptr->get_object_request.GetVersionId().empty());
  REQUIRE(!mock_s3_request_sender_ptr->get_object_request.VersionIdHasBeenSet());
  REQUIRE(mock_s3_request_sender_ptr->get_object_request.GetRequestPayer() == Aws::S3::Model::RequestPayer::NOT_SET);
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test subdirectories on AWS", "[awsS3Config]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Object Key", "dir1/dir2/logs.txt");
  test_controller.runSession(plan, true);
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:filename value:logs.txt"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:path value:dir1/dir2"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:absolute.path value:dir1/dir2/logs.txt"));
  REQUIRE(get_content(output_dir / INPUT_FILENAME).empty());
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test optional values are set in request", "[awsS3Config]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Version", S3_VERSION_1);
  plan->setProperty(s3_processor, "Requester Pays", "true");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->get_object_request.GetVersionId() == S3_VERSION_1);
  REQUIRE(mock_s3_request_sender_ptr->get_object_request.GetRequestPayer() == Aws::S3::Model::RequestPayer::requester);
}

TEST_CASE_METHOD(FetchS3ObjectTestsFixture, "Test non-default client configuration values", "[awsS3Config]") {
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

}  // namespace
