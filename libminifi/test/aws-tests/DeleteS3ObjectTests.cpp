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

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test no bucket is set") {
    setBasicCredentials();
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test success case", "[awsS3DeleteSuccess]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Version", "v1");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->bucket_name == "testBucket");
  REQUIRE(mock_s3_wrapper_ptr->object_key == INPUT_FILENAME);
  REQUIRE(mock_s3_wrapper_ptr->version == "v1");
}
