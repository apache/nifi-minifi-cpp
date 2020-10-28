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
#include "processors/ListS3.h"

using ListS3TestsFixture = FlowProducerS3TestsFixture<minifi::aws::processors::ListS3>;

TEST_CASE_METHOD(ListS3TestsFixture, "Test AWS credential setting", "[awsCredentials]") {
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

TEST_CASE_METHOD(ListS3TestsFixture, "Test required property not set", "[awsS3Errors]") {
  SECTION("Test no bucket is set") {
    setBasicCredentials();
  }

  SECTION("Test region is empty") {
    setRequiredProperties();
    plan->setProperty(s3_processor, "Region", "");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  setProxy();
  test_controller.runSession(plan, true);
  checkProxySettings();
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test listing without versioning", "[awsS3ListObjects]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);

  for (auto i = 1; i <= S3_OBJECT_COUNT; ++i) {
    REQUIRE(LogTestController::getInstance().contains("key:filename value:" + S3_KEY_PREFIX + std::to_string(i)));
    REQUIRE(LogTestController::getInstance().contains("key:s3.etag value:" + S3_ETAG_PREFIX + std::to_string(i)));
  }

  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.bucket value:" + S3_BUCKET) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.isLatest value:true") == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:" + std::to_string(S3_OBJECT_AGE_MILLISECONDS)) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.length value:" + std::to_string(S3_OBJECT_SIZE)) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.version") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.storeClass value:" + S3_STORAGE_CLASS_STR) == S3_OBJECT_COUNT);
}
