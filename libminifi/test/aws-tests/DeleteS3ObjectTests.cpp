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

#include <stdlib.h>
#include <iostream>
#include <map>

#include "core/Processor.h"
#include "../TestBase.h"
#include "processors/DeleteS3Object.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "s3/S3WrapperBase.h"
#include "utils/file/FileUtils.h"
#include "MockS3Wrapper.h"

class DeleteS3ObjectTestsFixture {
 public:
  DeleteS3ObjectTestsFixture() {
    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::aws::processors::DeleteS3Object>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_wrapper_ptr = new MockS3Wrapper();
    std::unique_ptr<minifi::aws::s3::S3WrapperBase> mock_s3_wrapper(mock_s3_wrapper_ptr);
    delete_s3_object = std::make_shared<minifi::aws::processors::DeleteS3Object>("DeleteS3Object", utils::Identifier(), std::move(mock_s3_wrapper));

    plan->addProcessor(
      "GenerateFlowFile",
      "GenerateFlowFile");
    update_attribute = plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      delete_s3_object,
      "DeleteS3Object",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
  }

  void setBasicCredentials() {
    plan->setProperty(delete_s3_object, "Access Key", "key");
    plan->setProperty(delete_s3_object, "Secret Key", "secret");
  }

  void setBucket() {
    plan->setProperty(update_attribute, "test.bucket", "testBucket", true);
    plan->setProperty(delete_s3_object, "Bucket", "${test.bucket}");
  }

  void setRequiredProperties() {
    setBasicCredentials();
    setBucket();
  }

  virtual ~DeleteS3ObjectTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  MockS3Wrapper* mock_s3_wrapper_ptr;
  std::shared_ptr<core::Processor> delete_s3_object;
  std::shared_ptr<core::Processor> update_attribute;
};

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test no bucket is set") {
    setBasicCredentials();
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(DeleteS3ObjectTestsFixture, "Test success case", "[awsS3DeleteSuccess]") {
  setRequiredProperties();
  plan->setProperty(delete_s3_object, "Object Key", "object");
  plan->setProperty(delete_s3_object, "Version", "v1");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->bucket_name == "testBucket");
  REQUIRE(mock_s3_wrapper_ptr->object_key == "object");
  REQUIRE(mock_s3_wrapper_ptr->version == "v1");
}
