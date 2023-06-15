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
#include "../processors/DeleteGCSObject.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "GCPAttributes.h"
#include "core/Resource.h"
#include "SingleProcessorTestController.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"

namespace gcs = ::google::cloud::storage;
namespace minifi_gcp = org::apache::nifi::minifi::extensions::gcp;

using DeleteGCSObject = org::apache::nifi::minifi::extensions::gcp::DeleteGCSObject;
using GCPCredentialsControllerService = org::apache::nifi::minifi::extensions::gcp::GCPCredentialsControllerService;
using DeleteObjectRequest = gcs::internal::DeleteObjectRequest;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;

namespace {
class DeleteGCSObjectMocked : public DeleteGCSObject {
  using org::apache::nifi::minifi::extensions::gcp::DeleteGCSObject::DeleteGCSObject;
 public:
  static constexpr const char* Description = "DeleteGCSObjectMocked";

  gcs::Client getClient() const override {
    return gcs::testing::UndecoratedClientFromMock(mock_client_);
  }
  std::shared_ptr<gcs::testing::MockClient> mock_client_ = std::make_shared<gcs::testing::MockClient>();
};
REGISTER_RESOURCE(DeleteGCSObjectMocked, Processor);
}  // namespace

class DeleteGCSObjectTests : public ::testing::Test {
 public:
  void SetUp() override {
    gcp_credentials_node_ = test_controller_.plan->addController("GCPCredentialsControllerService", "gcp_credentials_controller_service");
    test_controller_.plan->setProperty(gcp_credentials_node_,
                                       GCPCredentialsControllerService::CredentialsLoc,
                                       toString(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS));
    test_controller_.plan->setProperty(delete_gcs_object_,
                                       DeleteGCSObject::GCPCredentials,
                                       "gcp_credentials_controller_service");
  }
  std::shared_ptr<DeleteGCSObjectMocked> delete_gcs_object_ = std::make_shared<DeleteGCSObjectMocked>("DeleteGCSObjectMocked");
  org::apache::nifi::minifi::test::SingleProcessorTestController test_controller_{delete_gcs_object_};
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  gcp_credentials_node_;
};

TEST_F(DeleteGCSObjectTests, MissingBucket) {
  EXPECT_CALL(*delete_gcs_object_->mock_client_, CreateResumableUpload).Times(0);
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::Bucket, ""));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(0, result.at(DeleteGCSObject::Success).size());
  ASSERT_EQ(1, result.at(DeleteGCSObject::Failure).size());
  EXPECT_EQ(std::nullopt, result.at(DeleteGCSObject::Failure)[0]->getAttribute(std::string(minifi_gcp::GCS_ERROR_DOMAIN)));
  EXPECT_EQ(std::nullopt, result.at(DeleteGCSObject::Failure)[0]->getAttribute(std::string(minifi_gcp::GCS_ERROR_REASON)));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(DeleteGCSObject::Failure)[0]));
}

TEST_F(DeleteGCSObjectTests, ServerGivesPermaError) {
  EXPECT_CALL(*delete_gcs_object_->mock_client_, DeleteObject)
      .WillOnce(testing::Return(PermanentError()));
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::Bucket, "bucket-from-property"));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(0, result.at(DeleteGCSObject::Success).size());
  ASSERT_EQ(1, result.at(DeleteGCSObject::Failure).size());
  EXPECT_NE(std::nullopt, result.at(DeleteGCSObject::Failure)[0]->getAttribute(std::string(minifi_gcp::GCS_ERROR_DOMAIN)));
  EXPECT_NE(std::nullopt, result.at(DeleteGCSObject::Failure)[0]->getAttribute(std::string(minifi_gcp::GCS_ERROR_REASON)));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(DeleteGCSObject::Failure)[0]));
}

TEST_F(DeleteGCSObjectTests, ServerGivesTransientErrors) {
  EXPECT_CALL(*delete_gcs_object_->mock_client_, DeleteObject).WillOnce(testing::Return(TransientError()));
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::NumberOfRetries, "1"));
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::Bucket, "bucket-from-property"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}});
  EXPECT_EQ(0, result.at(DeleteGCSObject::Success).size());
  ASSERT_EQ(1, result.at(DeleteGCSObject::Failure).size());
  EXPECT_NE(std::nullopt, result.at(DeleteGCSObject::Failure)[0]->getAttribute(std::string(minifi_gcp::GCS_ERROR_DOMAIN)));
  EXPECT_NE(std::nullopt, result.at(DeleteGCSObject::Failure)[0]->getAttribute(std::string(minifi_gcp::GCS_ERROR_REASON)));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(DeleteGCSObject::Failure)[0]));
}


TEST_F(DeleteGCSObjectTests, HandlingSuccessfullDeletion) {
  EXPECT_CALL(*delete_gcs_object_->mock_client_, DeleteObject)
      .WillOnce([](DeleteObjectRequest const& request) {
        EXPECT_EQ("bucket-from-attribute", request.bucket_name());
        EXPECT_TRUE(request.HasOption<gcs::Generation>());
        EXPECT_TRUE(request.GetOption<gcs::Generation>().has_value());
        EXPECT_EQ(23, request.GetOption<gcs::Generation>().value());
        return google::cloud::make_status_or(gcs::internal::EmptyResponse{});
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::ObjectGeneration, "${gcs.generation}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}, {std::string(minifi_gcp::GCS_GENERATION), "23"}});
  ASSERT_EQ(1, result.at(DeleteGCSObject::Success).size());
  EXPECT_EQ(0, result.at(DeleteGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(DeleteGCSObject::Success)[0]));
}

TEST_F(DeleteGCSObjectTests, EmptyGeneration) {
  EXPECT_CALL(*delete_gcs_object_->mock_client_, DeleteObject)
      .WillOnce([](DeleteObjectRequest const& request) {
        EXPECT_EQ("bucket-from-attribute", request.bucket_name());
        EXPECT_FALSE(request.HasOption<gcs::Generation>());
        return google::cloud::make_status_or(gcs::internal::EmptyResponse{});
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::ObjectGeneration, "${gcs.generation}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}});
  ASSERT_EQ(1, result.at(DeleteGCSObject::Success).size());
  EXPECT_EQ(0, result.at(DeleteGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(DeleteGCSObject::Success)[0]));
}

TEST_F(DeleteGCSObjectTests, InvalidGeneration) {
  EXPECT_TRUE(test_controller_.plan->setProperty(delete_gcs_object_, DeleteGCSObject::ObjectGeneration, "${gcs.generation}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}, {std::string(minifi_gcp::GCS_GENERATION), "23 banana"}});
  ASSERT_EQ(0, result.at(DeleteGCSObject::Success).size());
  EXPECT_EQ(1, result.at(DeleteGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(DeleteGCSObject::Failure)[0]));
}
