/**
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
#include "../processors/PutGCSObject.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "GCPAttributes.h"
#include "core/Resource.h"
#include "unit/SingleProcessorTestController.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "unit/ProcessorUtils.h"

namespace gcs = ::google::cloud::storage;
namespace minifi_gcp = org::apache::nifi::minifi::extensions::gcp;

using PutGCSObject = org::apache::nifi::minifi::extensions::gcp::PutGCSObject;
using GCPCredentialsControllerService = org::apache::nifi::minifi::extensions::gcp::GCPCredentialsControllerService;
using ResumableUploadRequest = gcs::internal::ResumableUploadRequest;
using QueryResumableUploadResponse = gcs::internal::QueryResumableUploadResponse;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;

namespace {
class PutGCSObjectMocked : public PutGCSObject {
  using org::apache::nifi::minifi::extensions::gcp::PutGCSObject::PutGCSObject;
 public:
  static constexpr const char* Description = "PutGCSObjectMocked";

  gcs::Client getClient() const override {
    return gcs::testing::UndecoratedClientFromMock(mock_client_);
  }
  std::shared_ptr<gcs::testing::MockClient> mock_client_ = std::make_shared<gcs::testing::MockClient>();
};
REGISTER_RESOURCE(PutGCSObjectMocked, Processor);
}  // namespace

class PutGCSObjectTests : public ::testing::Test {
 public:
  void SetUp() override {
    put_gcs_object_ = test_controller_.getProcessor();
    gcp_credentials_node_ = test_controller_.plan->addController("GCPCredentialsControllerService", "gcp_credentials_controller_service");
    test_controller_.plan->setProperty(gcp_credentials_node_,
                                       GCPCredentialsControllerService::CredentialsLoc,
                                       magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS));
    test_controller_.plan->setProperty(put_gcs_object_,
                                       PutGCSObject::GCPCredentials,
                                       "gcp_credentials_controller_service");
  }
  TypedProcessorWrapper<PutGCSObjectMocked> put_gcs_object_;
  org::apache::nifi::minifi::test::SingleProcessorTestController test_controller_{minifi::test::utils::make_processor<PutGCSObjectMocked>("PutGCSObjectMocked")};
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  gcp_credentials_node_;

  static auto return_upload_done(const ResumableUploadRequest& request) {
    using ObjectMetadataParser = gcs::internal::ObjectMetadataParser;
    nlohmann::json metadata_json;
    metadata_json["name"] = request.object_name();
    metadata_json["bucket"] = request.bucket_name();
    metadata_json["size"] = 10;
    if (request.HasOption<gcs::EncryptionKey>()) {
      metadata_json["customerEncryption"]["encryptionAlgorithm"] = "AES256";
      metadata_json["customerEncryption"]["keySha256"] = "zkeXIcAB56dkHp0z1023TQZ+mzm+fZ5JRVgmAQ3bEVE=";
    }
    return testing::Return(google::cloud::make_status_or(QueryResumableUploadResponse{absl::nullopt, *ObjectMetadataParser::FromJson(metadata_json)}));
  }
};

TEST_F(PutGCSObjectTests, MissingBucket) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload).Times(0);
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, ""));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(0, result.at(PutGCSObject::Success).size());
  ASSERT_EQ(1, result.at(PutGCSObject::Failure).size());
  EXPECT_EQ(std::nullopt, result.at(PutGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_DOMAIN));
  EXPECT_EQ(std::nullopt, result.at(PutGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_REASON));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Failure)[0]));
}

TEST_F(PutGCSObjectTests, BucketFromAttribute) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_EQ("bucket-from-attribute", request.bucket_name());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "${gcs.bucket}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}});
  ASSERT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Success)[0]));
}

TEST_F(PutGCSObjectTests, ServerGivesTransientErrors) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload).WillOnce(testing::Return(TransientError()));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::NumberOfRetries, "2"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(0, result.at(PutGCSObject::Success).size());
  ASSERT_EQ(1, result.at(PutGCSObject::Failure).size());
  EXPECT_NE(std::nullopt, result.at(PutGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_DOMAIN));
  EXPECT_NE(std::nullopt, result.at(PutGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_REASON));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Failure)[0]));
}

TEST_F(PutGCSObjectTests, ServerGivesPermaError) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload).WillOnce(testing::Return(PermanentError()));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(0, result.at(PutGCSObject::Success).size());
  ASSERT_EQ(1, result.at(PutGCSObject::Failure).size());
  EXPECT_NE(std::nullopt, result.at(PutGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_DOMAIN));
  EXPECT_NE(std::nullopt, result.at(PutGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_REASON));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Failure)[0]));
}

TEST_F(PutGCSObjectTests, NonRequiredPropertiesAreMissing) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_FALSE(request.HasOption<gcs::MD5HashValue>());
        EXPECT_FALSE(request.HasOption<gcs::Crc32cChecksumValue>());
        EXPECT_FALSE(request.HasOption<gcs::PredefinedAcl>());
        EXPECT_FALSE(request.HasOption<gcs::IfGenerationMatch>());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
}

TEST_F(PutGCSObjectTests, Crc32cMD5LocationTest) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_TRUE(request.HasOption<gcs::Crc32cChecksumValue>());
        EXPECT_EQ("yZRlqg==", request.GetOption<gcs::Crc32cChecksumValue>().value());
        EXPECT_TRUE(request.HasOption<gcs::MD5HashValue>());
        EXPECT_EQ("XrY7u+Ae7tCTyyK7j1rNww==", request.GetOption<gcs::MD5HashValue>().value());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::MD5Hash, "${md5}"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Crc32cChecksum, "${crc32c}"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world", {{"crc32c", "yZRlqg=="}, {"md5", "XrY7u+Ae7tCTyyK7j1rNww=="}});
  EXPECT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
}

TEST_F(PutGCSObjectTests, DontOverwriteTest) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_TRUE(request.HasOption<gcs::IfGenerationMatch>());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::OverwriteObject, "false"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world", {{"crc32c", "yZRlqg=="}, {"md5", "XrY7u+Ae7tCTyyK7j1rNww=="}});
  ASSERT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Success)[0]));
}

TEST_F(PutGCSObjectTests, ValidServerSideEncryptionTest) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_TRUE(request.HasOption<gcs::EncryptionKey>());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::EncryptionKey, "ZW5jcnlwdGlvbl9rZXk="));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world");
  ASSERT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
  EXPECT_NE(std::nullopt, result.at(PutGCSObject::Success)[0]->getAttribute(minifi_gcp::GCS_ENCRYPTION_SHA256_ATTR));
  EXPECT_NE(std::nullopt, result.at(PutGCSObject::Success)[0]->getAttribute(minifi_gcp::GCS_ENCRYPTION_ALGORITHM_ATTR));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Success)[0]));
}

TEST_F(PutGCSObjectTests, InvalidServerSideEncryptionTest) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload).Times(0);
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::EncryptionKey, "not_base64_key"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  EXPECT_THROW(test_controller_.trigger("hello world"), minifi::Exception);
}

TEST_F(PutGCSObjectTests, NoContentType) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_FALSE(request.HasOption<gcs::ContentType>());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world");
  ASSERT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Success)[0]));
}

TEST_F(PutGCSObjectTests, ContentTypeFromAttribute) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_TRUE(request.HasOption<gcs::ContentType>());
        EXPECT_EQ("text/attribute", request.GetOption<gcs::ContentType>().value());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  const auto& result = test_controller_.trigger("hello world", {{"mime.type", "text/attribute"}});
  ASSERT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Success)[0]));
}

TEST_F(PutGCSObjectTests, ObjectACLTest) {
  EXPECT_CALL(*put_gcs_object_.get().mock_client_, CreateResumableUpload)
      .WillOnce([this](const ResumableUploadRequest& request) {
        EXPECT_TRUE(request.HasOption<gcs::PredefinedAcl>());
        EXPECT_EQ(gcs::PredefinedAcl::AuthenticatedRead().value(), request.GetOption<gcs::PredefinedAcl>().value());
        EXPECT_CALL(*put_gcs_object_.get().mock_client_, UploadChunk).WillOnce(return_upload_done(request));
        return gcs::internal::CreateResumableUploadResponse{"test-only-upload-id"};
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::Key, "object-name-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(put_gcs_object_, PutGCSObject::ObjectACL, magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::AUTHENTICATED_READ)));
  const auto& result = test_controller_.trigger("hello world");
  ASSERT_EQ(1, result.at(PutGCSObject::Success).size());
  EXPECT_EQ(0, result.at(PutGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(PutGCSObject::Success)[0]));
}

TEST_F(PutGCSObjectTests, PredefinedACLTests) {
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::AUTHENTICATED_READ), gcs::PredefinedAcl::AuthenticatedRead().value());
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::BUCKET_OWNER_FULL_CONTROL), gcs::PredefinedAcl::BucketOwnerFullControl().value());
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::BUCKET_OWNER_READ_ONLY), gcs::PredefinedAcl::BucketOwnerRead().value());
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::PRIVATE), gcs::PredefinedAcl::Private().value());
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::PROJECT_PRIVATE), gcs::PredefinedAcl::ProjectPrivate().value());
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::PUBLIC_READ_ONLY), gcs::PredefinedAcl::PublicRead().value());
  EXPECT_EQ(magic_enum::enum_name(minifi_gcp::put_gcs_object::PredefinedAcl::PUBLIC_READ_WRITE), gcs::PredefinedAcl::PublicReadWrite().value());
}
