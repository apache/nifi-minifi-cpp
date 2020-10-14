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

class PutS3ObjectTestsFixture : public S3TestsFixture<minifi::aws::processors::PutS3Object> {
 public:
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
};

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test AWS credential setting", "[awsCredentials]") {
  setBucket();

  SECTION("Test property credentials") {
    plan->setProperty(update_attribute, "s3.accessKey", "key", true);
    plan->setProperty(s3_processor, "Access Key", "${s3.accessKey}");
    plan->setProperty(update_attribute, "s3.secretKey", "secret", true);
    plan->setProperty(s3_processor, "Secret Key", "${s3.secretKey}");
  }


  SECTION("Test credentials setting from AWS Credentials service") {
    auto aws_cred_service = plan->addController("AWSCredentialsService", "AWSCredentialsService");
    plan->setProperty(aws_cred_service, "Access Key", "key");
    plan->setProperty(aws_cred_service, "Secret Key", "secret");
    plan->setProperty(s3_processor, "AWS Credentials Provider service", "AWSCredentialsService");
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
    plan->setProperty(s3_processor, "Credentials File", aws_credentials_file);
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
    plan->setProperty(s3_processor, "AWS Credentials Provider service", "AWSCredentialsService");
  }

  SECTION("Test credentials setting using default credential chain") {
    #ifdef WIN32
    _putenv_s("AWS_ACCESS_KEY_ID", "key");
    _putenv_s("AWS_SECRET_ACCESS_KEY", "secret");
    #else
    setenv("AWS_ACCESS_KEY_ID", "key", 1);
    setenv("AWS_SECRET_ACCESS_KEY", "secret", 1);
    #endif
    plan->setProperty(s3_processor, "Use Default Credentials", "true");
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
    plan->setProperty(s3_processor, "AWS Credentials Provider service", "AWSCredentialsService");
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

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Check default client configuration", "[awsS3ClientConfig]") {
  setRequiredProperties();
  test_controller.runSession(plan, true);
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:" + INPUT_FILENAME));
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
  REQUIRE(mock_s3_wrapper_ptr->put_s3_data == INPUT_DATA);
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
  REQUIRE(LogTestController::getInstance().contains("key:s3.bucket value:testBucket"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.key value:custom_key"));
  REQUIRE(LogTestController::getInstance().contains("key:s3.contenttype value:application/tar"));
  REQUIRE(mock_s3_wrapper_ptr->content_type == "application/tar");
  REQUIRE(mock_s3_wrapper_ptr->storage_class == Aws::S3::Model::StorageClass::REDUCED_REDUNDANCY);
  REQUIRE(mock_s3_wrapper_ptr->server_side_encryption == Aws::S3::Model::ServerSideEncryption::AES256);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().region == minifi::aws::processors::region::US_EAST_1);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().connectTimeoutMs == 10000);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().endpointOverride == "http://localhost:1234");
  REQUIRE(mock_s3_wrapper_ptr->put_s3_data == INPUT_DATA);
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test single user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "meta_key", "meta_value", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->metadata_map.at("meta_key") == "meta_value");
  REQUIRE(LogTestController::getInstance().contains("key:s3.usermetadata value:meta_key=meta_value"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test multiple user metadata", "[awsS3MetaData]") {
  setRequiredProperties();
  plan->setProperty(s3_processor, "meta_key1", "meta_value1", true);
  plan->setProperty(s3_processor, "meta_key2", "meta_value2", true);
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->metadata_map.at("meta_key1") == "meta_value1");
  REQUIRE(mock_s3_wrapper_ptr->metadata_map.at("meta_key2") == "meta_value2");
  REQUIRE(LogTestController::getInstance().contains("key:s3.usermetadata value:meta_key1=meta_value1,meta_key2=meta_value2"));
}

TEST_CASE_METHOD(PutS3ObjectTestsFixture, "Test proxy setting", "[awsS3Proxy]") {
  setRequiredProperties();
  plan->setProperty(update_attribute, "test.proxyHost", "host", true);
  plan->setProperty(s3_processor, "Proxy Host", "${test.proxyHost}");
  plan->setProperty(update_attribute, "test.proxyPort", "1234", true);
  plan->setProperty(s3_processor, "Proxy Port", "${test.proxyPort}");
  plan->setProperty(update_attribute, "test.proxyUsername", "username", true);
  plan->setProperty(s3_processor, "Proxy Username", "${test.proxyUsername}");
  plan->setProperty(update_attribute, "test.proxyPassword", "password", true);
  plan->setProperty(s3_processor, "Proxy Password", "${test.proxyPassword}");
  test_controller.runSession(plan, true);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyHost == "host");
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyPort == 1234);
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyUserName == "username");
  REQUIRE(mock_s3_wrapper_ptr->getClientConfig().proxyPassword == "password");
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
  REQUIRE(mock_s3_wrapper_ptr->fullcontrol_user_list == "id=myuserid123, emailAddress=\"myuser@example.com\"");
  REQUIRE(mock_s3_wrapper_ptr->read_user_list == "id=myuserid456, emailAddress=\"myuser2@example.com\"");
  REQUIRE(mock_s3_wrapper_ptr->read_acl_user_list == "id=myuserid789, id=otheruser");
  REQUIRE(mock_s3_wrapper_ptr->write_acl_user_list == "emailAddress=\"myuser3@example.com\"");
  REQUIRE(mock_s3_wrapper_ptr->canned_acl == Aws::S3::Model::ObjectCannedACL::public_read_write);
}
