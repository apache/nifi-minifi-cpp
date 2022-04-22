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
#include "processors/PutS3Object.h"
#include "utils/IntegrationTestUtils.h"

namespace {

using org::apache::nifi::minifi::utils::verifyLogLinePresenceInPollTime;

class PutS3ObjectTestsFixture : public FlowProcessorS3TestsFixture<minifi::aws::processors::PutS3Object> {
 public:
  void checkPutObjectResults() {
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.version value:" + S3_VERSION_1));
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.etag value:" + S3_ETAG_UNQUOTED));
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.expiration value:" + S3_EXPIRATION_DATE));
    REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.sseAlgorithm value:" + S3_SSEALGORITHM_STR));
  }

  void checkEmptyPutObjectResults() {
    REQUIRE_FALSE(LogTestController::getInstance().contains("key:s3.version value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    REQUIRE_FALSE(LogTestController::getInstance().contains("key:s3.etag value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    REQUIRE_FALSE(LogTestController::getInstance().contains("key:s3.expiration value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    REQUIRE_FALSE(LogTestController::getInstance().contains("key:s3.sseAlgorithm value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  }
};

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test AWS credential setting", "[awsCredentials]") {
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

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test credentials not set") {
  }

  SECTION("Test no bucket is set") {
    setAccesKeyCredentialsInProcessor();
  }

  SECTION("Test no object key is set") {
    setRequiredProperties();
    plan->setProperty(update_attribute, "filename", "", true);
  }

  SECTION("Test storage class is empty") {
    setRequiredProperties();
    plan->setProperty(s3_processor, "Storage Class", "");
  }

  SECTION("Test region is empty") {
    setRequiredProperties();
    plan->setProperty(s3_processor, "Region", "");
  }

  SECTION("Test no server side encryption is set") {
    setRequiredProperties();
    plan->setProperty(s3_processor, "Server Side Encryption", "");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test incomplete credentials in credentials service", "[awsS3Config]") {
  setBucket();
  plan->setProperty(aws_credentials_service, "Secret Key", "secret");
  setCredentialsService();
  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "AWS Credentials have not been set!"));

  // Test that no invalid credentials file was set from previous properties
  REQUIRE_FALSE(LogTestController::getInstance().contains("load configure file failed", std::chrono::seconds(0), std::chrono::milliseconds(0)));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:" + INPUT_FILENAME));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/octet-stream"));
  checkPutObjectResults();
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetContentType() == "application/octet-stream");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetStorageClass() == Aws::S3::Model::StorageClass::STANDARD);
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetServerSideEncryption() == Aws::S3::Model::ServerSideEncryption::NOT_SET);
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetACL() == Aws::S3::Model::ObjectCannedACL::NOT_SET);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().region == minifi::aws::processors::region::US_WEST_2);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().connectTimeoutMs == 30000);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().endpointOverride.empty());
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyHost.empty());
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyUserName.empty());
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().proxyPassword.empty());
  REQUIRE(mock_s3_request_sender_ptr->getPutObjectRequestBody() == INPUT_DATA);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration with empty result", "[awsS3ClientConfig]") {
  setRequiredProperties();
  mock_s3_request_sender_ptr->returnEmptyS3Result();
  test_controller.runSession(plan, true);
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:input_data.log"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/octet-stream"));
  checkEmptyPutObjectResults();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Set non-default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Object Key", "custom_key");
  plan->setProperty(update_attribute, "test.contentType", "application/tar", true);
  plan->setProperty(s3_processor, "Content Type", "${test.contentType}");
  plan->setProperty(s3_processor, "Storage Class", "ReducedRedundancy");
  plan->setProperty(s3_processor, "Region", minifi::aws::processors::region::US_EAST_1);
  plan->setProperty(s3_processor, "Communications Timeout", "10 Sec");
  plan->setProperty(update_attribute, "test.endpoint", "http://localhost:1234", true);
  plan->setProperty(s3_processor, "Endpoint Override URL", "${test.endpoint}");
  plan->setProperty(s3_processor, "Server Side Encryption", "AES256");
  test_controller.runSession(plan, true);
  checkPutObjectResults();
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:custom_key"));
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/tar"));
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetContentType() == "application/tar");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetStorageClass() == Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY);
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetServerSideEncryption() == Aws::S3::Model::ServerSideEncryption::AES256);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().region == minifi::aws::processors::region::US_EAST_1);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().connectTimeoutMs == 10000);
  REQUIRE(mock_s3_request_sender_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
  REQUIRE(mock_s3_request_sender_ptr->getPutObjectRequestBody() == INPUT_DATA);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test single user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "meta_key", "meta_value", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetMetadata().at("meta_key") == "meta_value");
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.usermetadata value:meta_key=meta_value"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test multiple user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "meta_key1", "meta_value1", true);
  plan->setProperty(s3_processor, "meta_key2", "meta_value2", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetMetadata().at("meta_key1") == "meta_value1");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetMetadata().at("meta_key2") == "meta_value2");
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.usermetadata value:meta_key1=meta_value1,meta_key2=meta_value2"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  setProxy();
  test_controller.runSession(plan, true);
  checkProxySettings();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test access control setting", "[awsS3ACL]") {
  setRequiredProperties();
  plan->setProperty(update_attribute, "s3.permissions.full.users", "myuserid123, myuser@example.com", true);
  plan->setProperty(s3_processor, "FullControl User List", "${s3.permissions.full.users}");
  plan->setProperty(update_attribute, "s3.permissions.read.users", "myuserid456,myuser2@example.com", true);
  plan->setProperty(s3_processor, "Read Permission User List", "${s3.permissions.read.users}");
  plan->setProperty(update_attribute, "s3.permissions.readacl.users", "myuserid789, otheruser", true);
  plan->setProperty(s3_processor, "Read ACL User List", "${s3.permissions.readacl.users}");
  plan->setProperty(update_attribute, "s3.permissions.writeacl.users", "myuser3@example.com", true);
  plan->setProperty(s3_processor, "Write ACL User List", "${s3.permissions.writeacl.users}");
  plan->setProperty(update_attribute, "s3.permissions.cannedacl", "PublicReadWrite", true);
  plan->setProperty(s3_processor, "Canned ACL", "${s3.permissions.cannedacl}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetGrantFullControl() == "id=myuserid123, emailAddress=\"myuser@example.com\"");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetGrantRead() == "id=myuserid456, emailAddress=\"myuser2@example.com\"");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetGrantReadACP() == "id=myuserid789, id=otheruser");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetGrantWriteACP() == "emailAddress=\"myuser3@example.com\"");
  REQUIRE(mock_s3_request_sender_ptr->put_object_request.GetACL() == Aws::S3::Model::ObjectCannedACL::public_read_write);
}

}  // namespace
