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
#include "../processors/ListGCSBucket.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "core/Resource.h"
#include "unit/SingleProcessorTestController.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"

namespace gcs = ::google::cloud::storage;
namespace minifi_gcp = org::apache::nifi::minifi::extensions::gcp;

using ListGCSBucket = org::apache::nifi::minifi::extensions::gcp::ListGCSBucket;
using ListObjectsRequest = gcs::internal::ListObjectsRequest;
using ListObjectsResponse = gcs::internal::ListObjectsResponse;
using GCPCredentialsControllerService = org::apache::nifi::minifi::extensions::gcp::GCPCredentialsControllerService;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;

namespace {
class ListGCSBucketMocked : public ListGCSBucket {
  using org::apache::nifi::minifi::extensions::gcp::ListGCSBucket::ListGCSBucket;
 public:
  static constexpr const char* Description = "ListGCSBucketMocked";

  gcs::Client getClient() const override {
    return gcs::testing::UndecoratedClientFromMock(mock_client_);
  }
  std::shared_ptr<gcs::testing::MockClient> mock_client_ = std::make_shared<gcs::testing::MockClient>();
};
REGISTER_RESOURCE(ListGCSBucketMocked, Processor);

auto CreateObject(int index, int generation = 1) {
  std::string id = "object-" + std::to_string(index);
  std::string name = id;
  std::string link =
      "https://storage.googleapis.com/storage/v1/b/test-bucket/" + id + "#1";
  nlohmann::json metadata{
      {"bucket", "test-bucket"},
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"generation", generation},
      {"kind", "storage#object"},
  };
  return google::cloud::storage::internal::ObjectMetadataParser::FromJson(metadata).value();
}
}  // namespace

class ListGCSBucketTests : public ::testing::Test {
 public:
  void SetUp() override {
    list_gcs_bucket_ = test_controller_.getProcessor<ListGCSBucketMocked>();
    gcp_credentials_node_ = test_controller_.plan->addController("GCPCredentialsControllerService", "gcp_credentials_controller_service");
    test_controller_.plan->setProperty(gcp_credentials_node_,
                                       GCPCredentialsControllerService::CredentialsLoc,
                                       magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS));
    test_controller_.plan->setProperty(list_gcs_bucket_,
                                       ListGCSBucket::GCPCredentials,
                                       "gcp_credentials_controller_service");
  }
  org::apache::nifi::minifi::test::SingleProcessorTestController test_controller_{std::make_unique<ListGCSBucketMocked>("ListGCSBucketMocked")};
  ListGCSBucketMocked* list_gcs_bucket_ = nullptr;
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  gcp_credentials_node_;
};

TEST_F(ListGCSBucketTests, MissingBucket) {
  EXPECT_CALL(*list_gcs_bucket_->mock_client_, CreateResumableUpload).Times(0);
  EXPECT_THROW(test_controller_.trigger(), std::runtime_error);
}

TEST_F(ListGCSBucketTests, ServerGivesPermaError) {
  auto return_permanent_error = [](ListObjectsRequest const&) {
    return google::cloud::StatusOr<ListObjectsResponse>(PermanentError());
  };
  EXPECT_CALL(*list_gcs_bucket_->mock_client_, ListObjects)
      .WillOnce(return_permanent_error);
  EXPECT_TRUE(test_controller_.plan->setProperty(list_gcs_bucket_, ListGCSBucket::Bucket, "bucket-from-property"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(0, result.at(ListGCSBucket::Success).size());
}

TEST_F(ListGCSBucketTests, ServerGivesTransientErrors) {
  auto return_temp_error = [](ListObjectsRequest const&) {
    return google::cloud::StatusOr<ListObjectsResponse>(TransientError());
  };
  EXPECT_CALL(*list_gcs_bucket_->mock_client_, ListObjects).WillOnce(return_temp_error);
  EXPECT_TRUE(test_controller_.plan->setProperty(list_gcs_bucket_, ListGCSBucket::NumberOfRetries, "1"));
  EXPECT_TRUE(test_controller_.plan->setProperty(list_gcs_bucket_, ListGCSBucket::Bucket, "bucket-from-property"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(0, result.at(ListGCSBucket::Success).size());
}

TEST_F(ListGCSBucketTests, WithoutVersions) {
  EXPECT_CALL(*list_gcs_bucket_->mock_client_, ListObjects)
      .WillOnce([](ListObjectsRequest const& req)
                    -> google::cloud::StatusOr<ListObjectsResponse> {
        EXPECT_EQ("bucket-from-property", req.bucket_name());
        EXPECT_TRUE(req.HasOption<gcs::Versions>());
        EXPECT_FALSE(req.GetOption<gcs::Versions>().value());

        ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1, 1));
        response.items.emplace_back(CreateObject(1, 2));
        response.items.emplace_back(CreateObject(1, 3));
        return response;
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(list_gcs_bucket_, ListGCSBucket::Bucket, "bucket-from-property"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(3, result.at(ListGCSBucket::Success).size());
}


TEST_F(ListGCSBucketTests, WithVersions) {
  EXPECT_CALL(*list_gcs_bucket_->mock_client_, ListObjects)
      .WillOnce([](ListObjectsRequest const& req)
                    -> google::cloud::StatusOr<ListObjectsResponse> {
        EXPECT_EQ("bucket-from-property", req.bucket_name());
        EXPECT_TRUE(req.HasOption<gcs::Versions>());
        EXPECT_TRUE(req.GetOption<gcs::Versions>().value());

        ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(list_gcs_bucket_, ListGCSBucket::Bucket, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.plan->setProperty(list_gcs_bucket_, ListGCSBucket::ListAllVersions, "true"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(3, result.at(ListGCSBucket::Success).size());
}

