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
#include "processors/PutS3Object.h"
#include "processors/GetFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "s3/S3WrapperBase.h"
#include "utils/file/FileUtils.h"

const std::string S3_VERSION = "1.2.3";
const std::string S3_ETAG = "\"tag-123\"";
const std::string S3_ETAG_UNQUOTED = "tag-123";
const std::string S3_EXPIRATION = "expiry-date=\"Wed, 28 Oct 2020 00:00:00 GMT\", rule-id=\"my_expiration_rule\"";
const std::string S3_EXPIRATION_DATE = "Wed, 28 Oct 2020 00:00:00 GMT";
const Aws::S3::Model::ServerSideEncryption S3_SSEALGORITHM = Aws::S3::Model::ServerSideEncryption::aws_kms;
const std::string S3_SSEALGORITHM_STR = "aws_kms";

class MockS3Wrapper : public minifi::aws::s3::S3WrapperBase {
 public:
  Aws::Auth::AWSCredentials getCredentials() const {
    return credentials_;
  }

  Aws::Client::ClientConfiguration getClientConfig() const {
    return client_config_;
  }

  minifi::utils::optional<Aws::S3::Model::PutObjectResult> sendPutObjectRequest(const Aws::S3::Model::PutObjectRequest& request) override {
    std::istreambuf_iterator<char> buf_it;
    put_s3_data = std::string(std::istreambuf_iterator<char>(*request.GetBody()), buf_it);
    bucket_name = request.GetBucket();
    object_key = request.GetKey();
    storage_class = request.GetStorageClass();
    server_side_encryption = request.GetServerSideEncryption();
    metadata_map = request.GetMetadata();
    content_type = request.GetContentType();
    fullcontrol_user_list = request.GetGrantFullControl();
    read_user_list = request.GetGrantRead();
    read_acl_user_list = request.GetGrantReadACP();
    write_acl_user_list = request.GetGrantWriteACP();
    write_acl_user_list = request.GetGrantWriteACP();
    canned_acl = request.GetACL();

    if (!get_empty_result) {
      put_s3_result.SetVersionId(S3_VERSION);
      put_s3_result.SetETag(S3_ETAG);
      put_s3_result.SetExpiration(S3_EXPIRATION);
      put_s3_result.SetServerSideEncryption(S3_SSEALGORITHM);
    }
    return put_s3_result;
  }

  std::string bucket_name;
  std::string object_key;
  Aws::S3::Model::StorageClass storage_class;
  Aws::S3::Model::ServerSideEncryption server_side_encryption;
  Aws::S3::Model::PutObjectResult put_s3_result;
  std::string put_s3_data;
  std::map<std::string, std::string> metadata_map;
  std::string content_type;
  std::string fullcontrol_user_list;
  std::string read_user_list;
  std::string read_acl_user_list;
  std::string write_acl_user_list;
  Aws::S3::Model::ObjectCannedACL canned_acl;
  bool get_empty_result = false;
};

class PutS3ObjectTestsFixture {
 public:
  PutS3ObjectTestsFixture() {
    // Disable retrieving AWS metadata for tests
    #ifdef WIN32
    _putenv_s("AWS_EC2_METADATA_DISABLED", "true");
    #else
    setenv("AWS_EC2_METADATA_DISABLED", "true", 1);
    #endif

    LogTestController::getInstance().setDebug<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setTrace<processors::GetFile>();
    LogTestController::getInstance().setDebug<processors::UpdateAttribute>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setTrace<minifi::aws::processors::PutS3Object>();

    // Build MiNiFi processing graph
    plan = test_controller.createPlan();
    mock_s3_wrapper_ptr = new MockS3Wrapper();
    std::unique_ptr<minifi::aws::s3::S3WrapperBase> mock_s3_wrapper(mock_s3_wrapper_ptr);
    put_s3_object = std::make_shared<minifi::aws::processors::PutS3Object>("PutS3Object", utils::Identifier(), std::move(mock_s3_wrapper));

    char input_dir_mask[] = "/tmp/gt.XXXXXX";
    auto input_dir = test_controller.createTempDirectory(input_dir_mask);
    std::ofstream input_file_stream(input_dir + utils::file::FileUtils::get_separator() + "input_data.log");
    input_file_stream << "input_data";
    input_file_stream.close();
    get_file = plan->addProcessor("GetFile", "GetFile");
    plan->setProperty(get_file, processors::GetFile::Directory.getName(), input_dir);
    plan->setProperty(get_file, processors::GetFile::KeepSourceFile.getName(), "false");
    update_attribute = plan->addProcessor(
      "UpdateAttribute",
      "UpdateAttribute",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      put_s3_object,
      "PutS3Object",
      core::Relationship("success", "d"),
      true);
    plan->addProcessor(
      "LogAttribute",
      "LogAttribute",
      core::Relationship("success", "d"),
      true);
  }

  void setBasicCredentials() {
    plan->setProperty(put_s3_object, "Access Key", "key");
    plan->setProperty(put_s3_object, "Secret Key", "secret");
  }

  void setBucket() {
    plan->setProperty(update_attribute, "test.bucket", "testBucket", true);
    plan->setProperty(put_s3_object, "Bucket", "${test.bucket}");
  }

  void setRequiredProperties() {
    setBasicCredentials();
    setBucket();
  }

  void checkPutObjectResults() {
    REQUIRE(LogTestController::getInstance().contains("key:s3.version value:" + S3_VERSION));
    REQUIRE(LogTestController::getInstance().contains("key:s3.etag value:" + S3_ETAG_UNQUOTED));
    REQUIRE(LogTestController::getInstance().contains("key:s3.expiration value:" + S3_EXPIRATION_DATE));
    REQUIRE(LogTestController::getInstance().contains("key:s3.sseAlgorithm value:" + S3_SSEALGORITHM_STR));
  }

  void checkEmptyPutObjectResults() {
    REQUIRE(!LogTestController::getInstance().contains("key:s3.version value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    REQUIRE(!LogTestController::getInstance().contains("key:s3.etag value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    REQUIRE(!LogTestController::getInstance().contains("key:s3.expiration value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
    REQUIRE(!LogTestController::getInstance().contains("key:s3.sseAlgorithm value:", std::chrono::seconds(0), std::chrono::milliseconds(0)));
  }

  std::string createTempFile(const std::string& filename) {
    char temp_dir[] = "/tmp/gt.XXXXXX";
    auto temp_path = test_controller.createTempDirectory(temp_dir);
    REQUIRE(!temp_path.empty());
    std::string temp_file(temp_path + utils::file::FileUtils::get_separator() + filename);
    return temp_file;
  }

  virtual ~PutS3ObjectTestsFixture() {
    LogTestController::getInstance().reset();
  }

 protected:
  TestController test_controller;
  std::shared_ptr<TestPlan> plan;
  MockS3Wrapper* mock_s3_wrapper_ptr;
  std::shared_ptr<core::Processor> put_s3_object;
  std::shared_ptr<core::Processor> get_file;
  std::shared_ptr<core::Processor> update_attribute;
};

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test AWS credential setting", "[awsCredentials]") {
  setBucket();

  SECTION("Test property credentials") {
    plan->setProperty(update_attribute, "s3.accessKey", "key", true);
    plan->setProperty(put_s3_object, "Access Key", "${s3.accessKey}");
    plan->setProperty(update_attribute, "s3.secretKey", "secret", true);
    plan->setProperty(put_s3_object, "Secret Key", "${s3.secretKey}");
  }


  SECTION("Test credentials setting from AWS Credentials service") {
    auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
    plan->setProperty(aws_cred_service, "Access Key", "key");
    plan->setProperty(aws_cred_service, "Secret Key", "secret");
    plan->setProperty(put_s3_object, "AWS Credentials Provider service", "AWSCredentialsService");
  }

  SECTION("Test credentials file setting") {
    char in_dir[] = "/tmp/gt.XXXXXX";
    auto temp_path = test_controller.createTempDirectory(in_dir);
    REQUIRE(!temp_path.empty());
    std::string aws_credentials_file(temp_path + utils::file::FileUtils::get_separator() + "aws_creds.conf");
    std::ofstream aws_credentials_file_stream(aws_credentials_file);
    aws_credentials_file_stream << "accessKey=key" << std::endl;
    aws_credentials_file_stream << "secretKey=secret" << std::endl;
    aws_credentials_file_stream.close();
    plan->setProperty(put_s3_object, "Credentials File", aws_credentials_file);
  }

  SECTION("Test credentials file setting from AWS Credentials service") {
    char in_dir[] = "/tmp/gt.XXXXXX";
    auto temp_path = test_controller.createTempDirectory(in_dir);
    REQUIRE(!temp_path.empty());
    std::string aws_credentials_file(temp_path + utils::file::FileUtils::get_separator() + "aws_creds.conf");
    std::ofstream aws_credentials_file_stream(aws_credentials_file);
    aws_credentials_file_stream << "accessKey=key" << std::endl;
    aws_credentials_file_stream << "secretKey=secret" << std::endl;
    aws_credentials_file_stream.close();
    auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
    plan->setProperty(aws_cred_service, "Credentials File", aws_credentials_file);
    plan->setProperty(put_s3_object, "AWS Credentials Provider service", "AWSCredentialsService");
  }

  SECTION("Test credentials setting using default credential chain") {
    #ifdef WIN32
    _putenv_s("AWS_ACCESS_KEY_ID", "key");
    _putenv_s("AWS_SECRET_ACCESS_KEY", "secret");
    #else
    setenv("AWS_ACCESS_KEY_ID", "key", 1);
    setenv("AWS_SECRET_ACCESS_KEY", "secret", 1);
    #endif
    plan->setProperty(put_s3_object, "Use Default Credentials", "true");
  }

  SECTION("Test credentials setting from AWS Credentials service using default credential chain") {
    auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
    plan->setProperty(aws_cred_service, "Use Default Credentials", "true");
    #ifdef WIN32
    _putenv_s("AWS_ACCESS_KEY_ID", "key");
    _putenv_s("AWS_SECRET_ACCESS_KEY", "secret");
    #else
    setenv("AWS_ACCESS_KEY_ID", "key", 1);
    setenv("AWS_SECRET_ACCESS_KEY", "secret", 1);
    #endif
    plan->setProperty(put_s3_object, "AWS Credentials Provider service", "AWSCredentialsService");
  }

  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSAccessKeyId() == "key");
  REQUIRE(mock_s3_wrapper_ptr->getCredentials().GetAWSSecretKey() == "secret");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test required property not set", "[awsS3Config]") {
  SECTION("Test credentials not set") {
  }

  SECTION("Test no bucket is set") {
    setBasicCredentials();
  }

  SECTION("Test no object key is set") {
    setRequiredProperties();
    plan->setProperty(update_attribute, "filename", "", true);
  }

  SECTION("Test storage class is empty") {
    setRequiredProperties();
    plan->setProperty(put_s3_object, "Storage Class", "");
  }

  SECTION("Test region is empty") {
    setRequiredProperties();
    plan->setProperty(put_s3_object, "Region", "");
  }

  SECTION("Test no server side encryption is set") {
    setRequiredProperties();
    plan->setProperty(put_s3_object, "Server Side Encryption", "");
  }

  REQUIRE_THROWS_AS(test_controller.runSession(plan, true), minifi::Exception);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:input_data.log"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/octet-stream"));
  checkPutObjectResults();
  REQUIRE(mock_s3_wrapper_ptr->content_type == "application/octet-stream");
  REQUIRE(mock_s3_wrapper_ptr->storage_class == Aws::S3::Model::StorageClass::STANDARD);
  REQUIRE(mock_s3_wrapper_ptr->server_side_encryption == Aws::S3::Model::ServerSideEncryption::NOT_SET);
  REQUIRE(mock_s3_wrapper_ptr->canned_acl == Aws::S3::Model::ObjectCannedACL::NOT_SET);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().region == minifi::aws::processors::region::US_WEST_2);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().connectTimeoutMs == 30000);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().endpointOverride.empty());
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyHost.empty());
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyUserName.empty());
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyPassword.empty());
  REQUIRE(mock_s3_wrapper_ptr->put_s3_data == "input_data");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration with empty result", "[awsS3ClientConfig]") {
  setRequiredProperties();
  mock_s3_wrapper_ptr->get_empty_result = true;
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:input_data.log"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/octet-stream"));
  checkEmptyPutObjectResults();
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Set non-default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  plan->setProperty(put_s3_object, "Object Key", "custom_key");
  plan->setProperty(update_attribute, "test.contentType", "application/tar", true);
  plan->setProperty(put_s3_object, "Content Type", "${test.contentType}");
  plan->setProperty(put_s3_object, "Storage Class", "ReducedRedundancy");
  plan->setProperty(put_s3_object, "Region", minifi::aws::processors::region::US_EAST_1);
  plan->setProperty(put_s3_object, "Communications Timeout", "10 Sec");
  plan->setProperty(update_attribute, "test.endpoint", "http://localhost:1234", true);
  plan->setProperty(put_s3_object, "Endpoint Override URL", "${test.endpoint}");
  plan->setProperty(put_s3_object, "Server Side Encryption", "AES256");
  test_controller.runSession(plan, true);
  checkPutObjectResults();
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:custom_key"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/tar"));
  REQUIRE(mock_s3_wrapper_ptr->content_type == "application/tar");
  REQUIRE(mock_s3_wrapper_ptr->storage_class == Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY);
  REQUIRE(mock_s3_wrapper_ptr->server_side_encryption == Aws::S3::Model::ServerSideEncryption::AES256);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().region == minifi::aws::processors::region::US_EAST_1);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().connectTimeoutMs == 10000);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
  REQUIRE(mock_s3_wrapper_ptr->put_s3_data == "input_data");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test single user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setProperty(put_s3_object, "meta_key", "meta_value", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->metadata_map.at("meta_key") == "meta_value");
  REQUIRE(LogTestController::getInstance().contains("key:s3.usermetadata value:meta_key=meta_value"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test multiple user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setProperty(put_s3_object, "meta_key1", "meta_value1", true);
  plan->setProperty(put_s3_object, "meta_key2", "meta_value2", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->metadata_map.at("meta_key1") == "meta_value1");
  REQUIRE(mock_s3_wrapper_ptr->metadata_map.at("meta_key2") == "meta_value2");
  REQUIRE(LogTestController::getInstance().contains("key:s3.usermetadata value:meta_key1=meta_value1,meta_key2=meta_value2"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  plan->setProperty(update_attribute, "test.proxyHost", "host", true);
  plan->setProperty(put_s3_object, "Proxy Host", "${test.proxyHost}");
  plan->setProperty(update_attribute, "test.proxyPort", "1234", true);
  plan->setProperty(put_s3_object, "Proxy Port", "${test.proxyPort}");
  plan->setProperty(update_attribute, "test.proxyUsername", "username", true);
  plan->setProperty(put_s3_object, "Proxy Username", "${test.proxyUsername}");
  plan->setProperty(update_attribute, "test.proxyPassword", "password", true);
  plan->setProperty(put_s3_object, "Proxy Password", "${test.proxyPassword}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyHost == "host");
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyPort == 1234);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyUserName == "username");
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyPassword == "password");
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test access control setting", "[awsS3ACL]") {
  setRequiredProperties();
  plan->setProperty(update_attribute, "s3.permissions.full.users", "myuserid123, myuser@example.com", true);
  plan->setProperty(put_s3_object, "FullControl User List", "${s3.permissions.full.users}");
  plan->setProperty(update_attribute, "s3.permissions.read.users", "myuserid456,myuser2@example.com", true);
  plan->setProperty(put_s3_object, "Read Permission User List", "${s3.permissions.read.users}");
  plan->setProperty(update_attribute, "s3.permissions.readacl.users", "myuserid789, otheruser", true);
  plan->setProperty(put_s3_object, "Read ACL User List", "${s3.permissions.readacl.users}");
  plan->setProperty(update_attribute, "s3.permissions.writeacl.users", "myuser3@example.com", true);
  plan->setProperty(put_s3_object, "Write ACL User List", "${s3.permissions.writeacl.users}");
  plan->setProperty(update_attribute, "s3.permissions.cannedacl", "PublicReadWrite", true);
  plan->setProperty(put_s3_object, "Canned ACL", "${s3.permissions.cannedacl}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->fullcontrol_user_list == "id=myuserid123, emailAddress=\"myuser@example.com\"");
  REQUIRE(mock_s3_wrapper_ptr->read_user_list == "id=myuserid456, emailAddress=\"myuser2@example.com\"");
  REQUIRE(mock_s3_wrapper_ptr->read_acl_user_list == "id=myuserid789, id=otheruser");
  REQUIRE(mock_s3_wrapper_ptr->write_acl_user_list == "emailAddress=\"myuser3@example.com\"");
  REQUIRE(mock_s3_wrapper_ptr->canned_acl == Aws::S3::Model::ObjectCannedACL::public_read_write);
}
