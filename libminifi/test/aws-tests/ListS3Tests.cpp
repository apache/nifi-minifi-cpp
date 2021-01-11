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
    setAccesKeyCredentialsInProcessor();
  }

  SECTION("Test credentials file setting") {
    setAccesKeyCredentialsInProcessor();
  }

  SECTION("Test credentials setting from AWS Credential service") {
    setAccessKeyCredentialsInController();
    setCredentialsService();
  }

  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test required property not set", "[awsS3Errors]") {
  SECTION("Test no bucket is set") {
    setAccesKeyCredentialsInProcessor();
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

  for (auto i = 0; i < S3_OBJECT_COUNT; ++i) {
    REQUIRE(LogTestController::getInstance().contains("key:filename value:" + S3_KEY_PREFIX + std::to_string(i)));
    REQUIRE(LogTestController::getInstance().contains("key:s3.etag value:" + S3_ETAG_PREFIX + std::to_string(i)));
  }

  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.bucket value:" + S3_BUCKET) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.isLatest value:true") == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:") == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:" + std::to_string(S3_OBJECT_OLD_AGE_MILLISECONDS)) == S3_OBJECT_COUNT / 2);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.length value:" + std::to_string(S3_OBJECT_SIZE)) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.version") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.storeClass value:" + S3_STORAGE_CLASS_STR) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.tag") == 0);
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test listing with versioning", "[awsS3ListVersions]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Use Versions", "true");
  test_controller.runSession(plan, true);

  for (auto i = 0; i < S3_OBJECT_COUNT; ++i) {
    // 2 versions of every object
    REQUIRE(LogTestController::getInstance().countOccurrences("key:filename value:" + S3_KEY_PREFIX + std::to_string(i)) == 2);
    REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.etag value:" + S3_ETAG_PREFIX + std::to_string(i)) == 2);
  }

  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.version value:" + S3_VERSION_1) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.version value:" + S3_VERSION_2) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.bucket value:" + S3_BUCKET) == S3_OBJECT_COUNT * 2);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.isLatest value:true") == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.isLatest value:false") == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:") == S3_OBJECT_COUNT * 2);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:" + std::to_string(S3_OBJECT_OLD_AGE_MILLISECONDS)) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.length value:" + std::to_string(S3_OBJECT_SIZE)) == S3_OBJECT_COUNT * 2);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.storeClass value:" + S3_STORAGE_CLASS_STR) == S3_OBJECT_COUNT * 2);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.tag") == 0);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.user.metadata") == 0);
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test if optional request values are set without versioning", "[awsS3ListOptionalValues]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Delimiter", "/");
  plan->setProperty(s3_processor, "Prefix", "test/");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->list_object_request.GetDelimiter() == "/");
  REQUIRE(mock_s3_wrapper_ptr->list_object_request.GetPrefix() == "test/");
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test if optional request values are set with versioning", "[awsS3ListOptionalValues]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Delimiter", "/");
  plan->setProperty(s3_processor, "Prefix", "test/");
  plan->setProperty(s3_processor, "Use Versions", "true");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->list_version_request.GetDelimiter() == "/");
  REQUIRE(mock_s3_wrapper_ptr->list_version_request.GetPrefix() == "test/");
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test minimum age property handling with non-versioned objects", "[awsS3ListMinAge]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Minimum Object Age", "120 days");
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:") == S3_OBJECT_COUNT / 2);
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test minimum age property handling with versioned objects", "[awsS3ListMinAge]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Minimum Object Age", "120 days");
  plan->setProperty(s3_processor, "Use Versions", "true");
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.lastModified value:") == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.version value:" + S3_VERSION_1) == S3_OBJECT_COUNT);
  REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.version value:" + S3_VERSION_2) == 0);
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test write object tags", "[awsS3ListTags]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Write Object Tags", "true");
  test_controller.runSession(plan, true);
  for (const auto& tag : S3_OBJECT_TAGS) {
    REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.tag." + tag.first + " value:" + tag.second) == S3_OBJECT_COUNT);
  }
}

TEST_CASE_METHOD(ListS3TestsFixture, "Test write user metadata", "[awsS3ListMetadata]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Write User Metadata", "true");
  plan->setProperty(s3_processor, "Requester Pays", "true");
  test_controller.runSession(plan, true);
  for (const auto& metadata : S3_OBJECT_USER_METADATA) {
    REQUIRE(LogTestController::getInstance().countOccurrences("key:s3.user.metadata." + metadata.first + " value:" + metadata.second) == S3_OBJECT_COUNT);
  }
  REQUIRE(mock_s3_wrapper_ptr->get_object_request.GetRequestPayer() == Aws::S3::Model::RequestPayer::requester);
}
