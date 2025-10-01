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
#include "unit/TestUtils.h"

namespace {

using org::apache::nifi::minifi::test::utils::verifyLogLinePresenceInPollTime;

class PutS3ObjectTestsFixture : public FlowProcessorS3TestsFixture<minifi::aws::processors::PutS3Object> {
 public:
  static void checkPutObjectResults() {
    CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.version value:" + S3_VERSION_1));
    CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.etag value:" + S3_ETAG_UNQUOTED));
    CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.expiration value:" + S3_EXPIRATION_DATE));
    CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.sseAlgorithm value:" + S3_SSEALGORITHM_STR));
  }

  static void checkEmptyPutObjectResults() {
    CHECK_FALSE(LogTestController::getInstance().contains("key:s3.version value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    CHECK_FALSE(LogTestController::getInstance().contains("key:s3.etag value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    CHECK_FALSE(LogTestController::getInstance().contains("key:s3.expiration value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    CHECK_FALSE(LogTestController::getInstance().contains("key:s3.sseAlgorithm value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  }
};

class PutS3ObjectLimitChanged : public minifi::aws::processors::PutS3Object {
 protected:
  friend class ::FlowProcessorS3TestsFixture<PutS3ObjectLimitChanged>;

  using PutS3Object::PutS3Object;

  uint64_t getMinPartSize() const override {
    return 1;
  }
};

class PutS3ObjectUploadLimitChangedTestsFixture : public FlowProcessorS3TestsFixture<PutS3ObjectLimitChanged> {
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

  test_controller.runSession(plan);
  CHECK(mock_s3_request_sender_ptr->getCredentials().GetAWSAccessKeyId() == "key");
  CHECK(mock_s3_request_sender_ptr->getCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test credentials not set") {
  }

  SECTION("Test no bucket is set") {
    setAccesKeyCredentialsInProcessor();
  }

  SECTION("Test no object key is set") {
    setRequiredProperties();
    CHECK(plan->setDynamicProperty(update_attribute, "filename", ""));
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan), minifi::Exception);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Non blank properties", "[awsS3Config]") {
  setRequiredProperties();
  CHECK_FALSE(plan->setProperty(s3_processor, "Server Side Encryption", ""));
  CHECK_FALSE(plan->setProperty(s3_processor, "Storage Class", ""));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test incomplete credentials in credentials service", "[awsS3Config]") {
  setBucket();
  plan->setProperty(aws_credentials_service, "Secret Key", "secret");
  setCredentialsService();
  REQUIRE_THROWS_AS(test_controller.runSession(plan), minifi::Exception);
  REQUIRE(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "AWS Credentials have not been set!"));

  // Test that no invalid credentials file was set from previous properties
  REQUIRE_FALSE(LogTestController::getInstance().contains("load configure file failed", std::chrono::seconds(0), std::chrono::milliseconds(0)));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  test_controller.runSession(plan);
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:" + INPUT_FILENAME));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/octet-stream"));
  checkPutObjectResults();
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetContentType() == "application/octet-stream");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetStorageClass() == Aws::S3::Model::StorageClass::STANDARD);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetServerSideEncryption() == Aws::S3::Model::ServerSideEncryption::NOT_SET);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetACL() == Aws::S3::Model::ObjectCannedACL::NOT_SET);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().region == minifi::aws::processors::region::US_WEST_2);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().connectTimeoutMs == 30000);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().endpointOverride.empty());
  CHECK(mock_s3_request_sender_ptr->getClientConfig().proxyHost.empty());
  CHECK(mock_s3_request_sender_ptr->getClientConfig().proxyUserName.empty());
  CHECK(mock_s3_request_sender_ptr->getClientConfig().proxyPassword.empty());
  CHECK(mock_s3_request_sender_ptr->getPutObjectRequestBody() == INPUT_DATA);
  CHECK(mock_s3_request_sender_ptr->getUseVirtualAddressing());
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration with empty result", "[awsS3ClientConfig]") {
  setRequiredProperties();
  mock_s3_request_sender_ptr->returnEmptyS3Result();
  test_controller.runSession(plan);
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:input_data.log"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/octet-stream"));
  checkEmptyPutObjectResults();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Set non-default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Object Key", "custom_key");
  plan->setDynamicProperty(update_attribute, "test.contentType", "application/tar");
  plan->setProperty(s3_processor, "Content Type", "${test.contentType}");
  plan->setProperty(s3_processor, "Storage Class", "ReducedRedundancy");
  plan->setProperty(s3_processor, "Region", minifi::aws::processors::region::AP_SOUTHEAST_3);
  plan->setProperty(s3_processor, "Communications Timeout", "10 Sec");
  plan->setDynamicProperty(update_attribute, "test.endpoint", "http://localhost:1234");
  plan->setProperty(s3_processor, "Endpoint Override URL", "${test.endpoint}");
  plan->setProperty(s3_processor, "Server Side Encryption", "AES256");
  test_controller.runSession(plan);
  checkPutObjectResults();
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:custom_key"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/tar"));
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetContentType() == "application/tar");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetStorageClass() == Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetServerSideEncryption() == Aws::S3::Model::ServerSideEncryption::AES256);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().region == minifi::aws::processors::region::AP_SOUTHEAST_3);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().connectTimeoutMs == 10000);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
  CHECK(mock_s3_request_sender_ptr->getPutObjectRequestBody() == INPUT_DATA);
  CHECK(mock_s3_request_sender_ptr->getUseVirtualAddressing());
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test single user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setDynamicProperty(s3_processor, "meta_key", "meta_value");
  test_controller.runSession(plan);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetMetadata().at("meta_key") == "meta_value");
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.usermetadata value:meta_key=meta_value"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test multiple user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setDynamicProperty(s3_processor, "meta_key1", "meta_value1");
  plan->setDynamicProperty(s3_processor, "meta_key2", "meta_value2");
  test_controller.runSession(plan);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetMetadata().at("meta_key1") == "meta_value1");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetMetadata().at("meta_key2") == "meta_value2");
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.usermetadata value:meta_key1=meta_value1,meta_key2=meta_value2"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  SECTION("Use proxy configuration service") {
    setProxy(true);
  }
  SECTION("Use processor properties") {
    setProxy(false);
  }
  test_controller.runSession(plan);
  checkProxySettings();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test access control setting", "[awsS3ACL]") {
  setRequiredProperties();
  plan->setDynamicProperty(update_attribute, "s3.permissions.full.users", "myuserid123, myuser@example.com");
  plan->setProperty(s3_processor, "FullControl User List", "${s3.permissions.full.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.read.users", "myuserid456,myuser2@example.com");
  plan->setProperty(s3_processor, "Read Permission User List", "${s3.permissions.read.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.readacl.users", "myuserid789, otheruser");
  plan->setProperty(s3_processor, "Read ACL User List", "${s3.permissions.readacl.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.writeacl.users", "myuser3@example.com");
  plan->setProperty(s3_processor, "Write ACL User List", "${s3.permissions.writeacl.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.cannedacl", "PublicReadWrite");
  plan->setProperty(s3_processor, "Canned ACL", "${s3.permissions.cannedacl}");
  test_controller.runSession(plan);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetGrantFullControl() == "id=myuserid123, emailAddress=\"myuser@example.com\"");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetGrantRead() == "id=myuserid456, emailAddress=\"myuser2@example.com\"");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetGrantReadACP() == "id=myuserid789, id=otheruser");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetGrantWriteACP() == "emailAddress=\"myuser3@example.com\"");
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetACL() == Aws::S3::Model::ObjectCannedACL::public_read_write);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test path style access property", "[awsS3PathStyleAccess]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Use Path Style Access", "true");
  test_controller.runSession(plan);
  REQUIRE(!mock_s3_request_sender_ptr->getUseVirtualAddressing());
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test multipart upload limits", "[awsS3MultipartUpload]") {
  setRequiredProperties();
  SECTION("Multipart Threshold is below limit") {
    plan->setProperty(s3_processor, "Multipart Threshold", "4 MB");
  }

  SECTION("Multipart Threshold is above limit") {
    plan->setProperty(s3_processor, "Multipart Threshold", "51 GB");
  }

  SECTION("Multipart Part Size is below limit") {
    plan->setProperty(s3_processor, "Multipart Part Size", "4 MB");
  }

  SECTION("Multipart Part Size is above limit") {
    plan->setProperty(s3_processor, "Multipart Part Size", "51 GB");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan), std::runtime_error);
}

TEST_CASE_METHOD(PutS3ObjectUploadLimitChangedTestsFixture, "Test multipart upload", "[awsS3MultipartUpload]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Multipart Threshold", "35 B");
  plan->setProperty(s3_processor, "Multipart Part Size", "10 B");
  auto temp_dir = test_controller.createTempDirectory();
  plan->setProperty(s3_processor, "Temporary Directory Multipart State", temp_dir.string());

  plan->setDynamicProperty(update_attribute, "s3.permissions.full.users", "myuserid123, myuser@example.com");
  plan->setProperty(s3_processor, "FullControl User List", "${s3.permissions.full.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.read.users", "myuserid456,myuser2@example.com");
  plan->setProperty(s3_processor, "Read Permission User List", "${s3.permissions.read.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.readacl.users", "myuserid789, otheruser");
  plan->setProperty(s3_processor, "Read ACL User List", "${s3.permissions.readacl.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.writeacl.users", "myuser3@example.com");
  plan->setProperty(s3_processor, "Write ACL User List", "${s3.permissions.writeacl.users}");
  plan->setDynamicProperty(update_attribute, "s3.permissions.cannedacl", "PublicReadWrite");
  plan->setProperty(s3_processor, "Canned ACL", "${s3.permissions.cannedacl}");
  plan->setDynamicProperty(s3_processor, "meta_key1", "meta_value1");
  plan->setDynamicProperty(s3_processor, "meta_key2", "meta_value2");
  plan->setDynamicProperty(update_attribute, "test.contentType", "application/tar");
  plan->setProperty(s3_processor, "Content Type", "${test.contentType}");
  plan->setProperty(s3_processor, "Storage Class", "ReducedRedundancy");
  plan->setProperty(s3_processor, "Region", minifi::aws::processors::region::AP_SOUTHEAST_3);
  plan->setProperty(s3_processor, "Communications Timeout", "10 Sec");
  plan->setDynamicProperty(update_attribute, "test.endpoint", "http://localhost:1234");
  plan->setProperty(s3_processor, "Endpoint Override URL", "${test.endpoint}");
  plan->setProperty(s3_processor, "Server Side Encryption", "AES256");

  std::string object_key;
  SECTION("Successful upload on first try") {
    object_key = INPUT_FILENAME;
    test_controller.runSession(plan);
  }

  SECTION("Successful upload on second try continuing the first multipart upload") {
    plan->setProperty(s3_processor, "Object Key", "resumable_key");
    object_key = "resumable_key";
    auto log_failure = plan->addProcessor(
      "LogAttribute",
      "LogFailure",
      core::Relationship("failure", "d"));
    plan->addConnection(s3_processor, core::Relationship("failure", "d"), log_failure);
    log_failure->setAutoTerminatedRelationships(std::array{core::Relationship("success", "d")});
    mock_s3_request_sender_ptr->failOnPartOnce(3);
    test_controller.runSession(plan);
    CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "Failed to upload part 3 of 4"));
    plan->reset();
    LogTestController::getInstance().clear();
    test_controller.runSession(plan);
  }

  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.version value:" + S3_VERSION_1));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.etag value:" + S3_ETAG_UNQUOTED));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.expiration value:" + S3_EXPIRATION_DATE));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.sseAlgorithm value:" + S3_SSEALGORITHM_STR));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.bucket value:testBucket"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.key value:" + object_key));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.contenttype value:application/tar"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.permissions.cannedacl value:PublicReadWrite"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.permissions.full.users value:myuserid123, myuser@example.com"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.permissions.read.users value:myuserid456,myuser2@example.com"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.permissions.readacl.users value:myuserid789, otheruser"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.permissions.writeacl.users value:myuser3@example.com"));
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "key:s3.usermetadata value:meta_key1=meta_value1,meta_key2=meta_value2"));
  CHECK(mock_s3_request_sender_ptr->getClientConfig().region == minifi::aws::processors::region::AP_SOUTHEAST_3);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().connectTimeoutMs == 10000);
  CHECK(mock_s3_request_sender_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
  CHECK(mock_s3_request_sender_ptr->getClientConfig().proxyHost.empty());
  CHECK(mock_s3_request_sender_ptr->getClientConfig().proxyUserName.empty());
  CHECK(mock_s3_request_sender_ptr->getClientConfig().proxyPassword.empty());
  CHECK(mock_s3_request_sender_ptr->getUseVirtualAddressing());
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetBucket() == S3_BUCKET);
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetKey() == object_key);
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetGrantFullControl() == "id=myuserid123, emailAddress=\"myuser@example.com\"");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetGrantRead() == "id=myuserid456, emailAddress=\"myuser2@example.com\"");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetGrantReadACP() == "id=myuserid789, id=otheruser");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetGrantWriteACP() == "emailAddress=\"myuser3@example.com\"");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetACL() == Aws::S3::Model::ObjectCannedACL::public_read_write);
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetMetadata().at("meta_key1") == "meta_value1");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetMetadata().at("meta_key2") == "meta_value2");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetContentType() == "application/tar");
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetStorageClass() == Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY);
  CHECK(mock_s3_request_sender_ptr->create_multipart_upload_request.GetServerSideEncryption() == Aws::S3::Model::ServerSideEncryption::AES256);

  REQUIRE(mock_s3_request_sender_ptr->upload_part_requests.size() == 4);
  for (size_t i = 0; i < mock_s3_request_sender_ptr->upload_part_requests.size(); ++i) {
    const auto& upload_part_request = mock_s3_request_sender_ptr->upload_part_requests[i];
    CHECK(upload_part_request.GetBucket() == S3_BUCKET);
    CHECK(upload_part_request.GetKey() == object_key);
    CHECK(upload_part_request.GetPartNumber() == static_cast<int>(i + 1));
    CHECK(upload_part_request.GetUploadId() == S3_UPLOAD_ID);
  }
  CHECK(mock_s3_request_sender_ptr->getUploadPartRequestBody(mock_s3_request_sender_ptr->upload_part_requests[0]) == INPUT_DATA.substr(0, 10));
  CHECK(mock_s3_request_sender_ptr->getUploadPartRequestBody(mock_s3_request_sender_ptr->upload_part_requests[1]) == INPUT_DATA.substr(10, 10));
  CHECK(mock_s3_request_sender_ptr->getUploadPartRequestBody(mock_s3_request_sender_ptr->upload_part_requests[2]) == INPUT_DATA.substr(20, 10));
  const auto last_part = mock_s3_request_sender_ptr->getUploadPartRequestBody(mock_s3_request_sender_ptr->upload_part_requests[3]);
  CHECK(last_part.size() == INPUT_DATA.size() % 10);
  CHECK(last_part == INPUT_DATA.substr(30));

  const auto& parts = mock_s3_request_sender_ptr->complete_multipart_upload_request.GetMultipartUpload().GetParts();
  REQUIRE(parts.size() == 4);
  for (size_t i = 0; i < parts.size(); ++i) {
    CHECK(parts[i].GetPartNumber() == static_cast<int>(i + 1));
    CHECK(parts[i].GetETag() == "etag" + std::to_string(i + 1));
  }
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test ageoff functionality aborting obselete multipart uploads", "[awsS3MultipartUpload]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Multipart Upload AgeOff Interval", "1 sec");
  plan->setProperty(s3_processor, "Multipart Upload Max Age Threshold", "2 years");
  auto temp_dir = test_controller.createTempDirectory();
  plan->setProperty(s3_processor, "Temporary Directory Multipart State", temp_dir.string());
  test_controller.runSession(plan);
  REQUIRE(mock_s3_request_sender_ptr->abort_multipart_upload_requests.size() == 1);
  CHECK(mock_s3_request_sender_ptr->abort_multipart_upload_requests[0].GetBucket() == S3_BUCKET);
  CHECK(mock_s3_request_sender_ptr->abort_multipart_upload_requests[0].GetKey() == "old_key");
  CHECK(mock_s3_request_sender_ptr->abort_multipart_upload_requests[0].GetUploadId() == "upload2");
}

TEST_CASE_METHOD(PutS3ObjectUploadLimitChangedTestsFixture, "Local state is not kept after successful upload", "[awsS3MultipartUpload]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Multipart Threshold", "35 B");
  plan->setProperty(s3_processor, "Multipart Part Size", "10 B");
  plan->setProperty(s3_processor, "Object Key", "resumable_key");
  auto temp_dir = test_controller.createTempDirectory();
  plan->setProperty(s3_processor, "Temporary Directory Multipart State", temp_dir.string());
  auto log_failure = plan->addProcessor(
    "LogAttribute",
    "LogFailure",
    core::Relationship("failure", "d"));
  plan->addConnection(s3_processor, core::Relationship("failure", "d"), log_failure);
  log_failure->setAutoTerminatedRelationships(std::array{core::Relationship("success", "d")});
  mock_s3_request_sender_ptr->failOnPartOnce(3);
  test_controller.runSession(plan);
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "Failed to upload part 3 of 4"));
  plan->reset();
  LogTestController::getInstance().clear();
  test_controller.runSession(plan);
  plan->reset();
  LogTestController::getInstance().clear();
  test_controller.runSession(plan);

  const auto& parts = mock_s3_request_sender_ptr->complete_multipart_upload_request.GetMultipartUpload().GetParts();
  REQUIRE(parts.size() == 4);
  for (size_t i = 0; i < parts.size(); ++i) {
    CHECK(parts[i].GetPartNumber() == static_cast<int>(i + 1));
    CHECK(parts[i].GetETag() == "etag" + std::to_string(4 + i + 1));  // The second successful upload contains different parts
  }
}

TEST_CASE_METHOD(PutS3ObjectUploadLimitChangedTestsFixture, "Do not continue multipart upload that only exists in local cache but not in S3", "[awsS3MultipartUpload]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Multipart Threshold", "35 B");
  plan->setProperty(s3_processor, "Multipart Part Size", "10 B");
  plan->setProperty(s3_processor, "Object Key", "non_resumable_key");
  auto temp_dir = test_controller.createTempDirectory();
  plan->setProperty(s3_processor, "Temporary Directory Multipart State", temp_dir.string());
  auto log_failure = plan->addProcessor(
    "LogAttribute",
    "LogFailure",
    core::Relationship("failure", "d"));
  plan->addConnection(s3_processor, core::Relationship("failure", "d"), log_failure);
  log_failure->setAutoTerminatedRelationships(std::array{core::Relationship("success", "d")});
  mock_s3_request_sender_ptr->failOnPartOnce(3);
  test_controller.runSession(plan);
  CHECK(verifyLogLinePresenceInPollTime(std::chrono::seconds(3), "Failed to upload part 3 of 4"));
  plan->reset();
  LogTestController::getInstance().clear();
  test_controller.runSession(plan);

  const auto& parts = mock_s3_request_sender_ptr->complete_multipart_upload_request.GetMultipartUpload().GetParts();
  REQUIRE(parts.size() == 4);
  for (size_t i = 0; i < parts.size(); ++i) {
    CHECK(parts[i].GetPartNumber() == static_cast<int>(i + 1));
    CHECK(parts[i].GetETag() == "etag" + std::to_string(2 + i + 1));  // The second successful upload contains different parts
  }
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test checksum algorithm property", "[awsS3checksum]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "Checksum Algorithm", "SHA256");
  test_controller.runSession(plan);
  CHECK(mock_s3_request_sender_ptr->put_object_request.GetChecksumAlgorithm() == Aws::S3::Model::ChecksumAlgorithm::SHA256);
}

}  // namespace
