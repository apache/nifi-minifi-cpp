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
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "../processors/ListGCSBucket.h"
#include "CProcessorTestUtils.h"
#include "core/Resource.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "unit/ProcessorUtils.h"
#include "unit/SingleProcessorTestController.h"

namespace gcs = ::google::cloud::storage;
namespace minifi_gcp = minifi::extensions::gcp;

using ListGCSBucket = minifi::extensions::gcp::ListGCSBucket;
using ListObjectsRequest = gcs::internal::ListObjectsRequest;
using ListObjectsResponse = gcs::internal::ListObjectsResponse;
using GCPCredentialsControllerService = minifi::extensions::gcp::GCPCredentialsControllerService;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;

namespace {
class ListGCSBucketMocked : public ListGCSBucket {
  using ListGCSBucket::ListGCSBucket;

 public:
  ListGCSBucketMocked(minifi::core::ProcessorMetadata metadata, std::shared_ptr<gcs::testing::MockClient> mock_client)
      : ListGCSBucket(std::move(metadata)),
        mock_client_(std::move(mock_client)) {}

 protected:
  gcs::Client getClient() const override { return gcs::testing::UndecoratedClientFromMock(mock_client_); }
  std::shared_ptr<gcs::testing::MockClient> mock_client_;
};

auto CreateObject(int index, int generation = 1) {
  std::string id = "object-" + std::to_string(index);
  std::string name = id;
  std::string link = "https://storage.googleapis.com/storage/v1/b/test-bucket/" + id + "#1";
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
 protected:
  void SetUp() override {
    const auto gcp_credential_controller_service =
        minifi::test::utils::make_custom_c_controller_service<GCPCredentialsControllerService>(core::ControllerServiceMetadata{utils::Identifier{},
            "GCPCredentialsControllerService",
            logging::LoggerFactory<GCPCredentialsControllerService>::getLogger()});
    gcp_credentials_node_ = test_controller_.plan->addController("gcp_credentials_controller_service", gcp_credential_controller_service);
    test_controller_.getProcessor()->setProperty(GCPCredentialsControllerService::CredentialsLoc.name,
        std::string(magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS)));
    test_controller_.getProcessor()->setProperty(ListGCSBucketMocked::GCPCredentials.name, "gcp_credentials_controller_service");
  }

 public:
  std::shared_ptr<gcs::testing::MockClient> mock_client_ = std::make_shared<gcs::testing::MockClient>();
  minifi::test::SingleProcessorTestController test_controller_{minifi::test::utils::make_custom_c_processor<ListGCSBucketMocked>(
      core::ProcessorMetadata{utils::Identifier{}, "ListGCSBucketMocked", logging::LoggerFactory<ListGCSBucketMocked>::getLogger()}, mock_client_)};
  std::shared_ptr<minifi::core::controller::ControllerServiceNode> gcp_credentials_node_;
};

TEST_F(ListGCSBucketTests, MissingBucket) {
  EXPECT_CALL(*mock_client_, CreateResumableUpload).Times(0);
  EXPECT_THROW(test_controller_.trigger(), std::runtime_error);
}

TEST_F(ListGCSBucketTests, ServerGivesPermaError) {
  auto return_permanent_error = [](ListObjectsRequest const&) { return google::cloud::StatusOr<ListObjectsResponse>(PermanentError()); };
  EXPECT_CALL(*mock_client_, ListObjects).WillOnce(return_permanent_error);
  EXPECT_TRUE(test_controller_.getProcessor()->setProperty(ListGCSBucket::Bucket.name, "bucket-from-property"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(0, result.at(ListGCSBucket::Success).size());
}

TEST_F(ListGCSBucketTests, ServerGivesTransientErrors) {
  auto return_temp_error = [](ListObjectsRequest const&) { return google::cloud::StatusOr<ListObjectsResponse>(TransientError()); };
  EXPECT_CALL(*mock_client_, ListObjects).WillOnce(return_temp_error);
  EXPECT_TRUE(test_controller_.getProcessor()->setProperty(ListGCSBucket::NumberOfRetries.name, "1"));
  EXPECT_TRUE(test_controller_.getProcessor()->setProperty(ListGCSBucket::Bucket.name, "bucket-from-property"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(0, result.at(ListGCSBucket::Success).size());
}

TEST_F(ListGCSBucketTests, WithoutVersions) {
  EXPECT_CALL(*mock_client_, ListObjects).WillOnce([](ListObjectsRequest const& req) -> google::cloud::StatusOr<ListObjectsResponse> {
    EXPECT_EQ("bucket-from-property", req.bucket_name());
    EXPECT_TRUE(req.HasOption<gcs::Versions>());
    EXPECT_FALSE(req.GetOption<gcs::Versions>().value());

    ListObjectsResponse response;
    response.items.emplace_back(CreateObject(1, 1));
    response.items.emplace_back(CreateObject(1, 2));
    response.items.emplace_back(CreateObject(1, 3));
    return response;
  });
  EXPECT_TRUE(test_controller_.getProcessor()->setProperty(ListGCSBucket::Bucket.name, "bucket-from-property"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(3, result.at(ListGCSBucket::Success).size());
}

TEST_F(ListGCSBucketTests, WithVersions) {
  EXPECT_CALL(*mock_client_, ListObjects).WillOnce([](ListObjectsRequest const& req) -> google::cloud::StatusOr<ListObjectsResponse> {
    EXPECT_EQ("bucket-from-property", req.bucket_name());
    EXPECT_TRUE(req.HasOption<gcs::Versions>());
    EXPECT_TRUE(req.GetOption<gcs::Versions>().value());

    ListObjectsResponse response;
    response.items.emplace_back(CreateObject(1));
    response.items.emplace_back(CreateObject(2));
    response.items.emplace_back(CreateObject(3));
    return response;
  });
  EXPECT_TRUE(test_controller_.getProcessor()->setProperty(ListGCSBucket::Bucket.name, "bucket-from-property"));
  EXPECT_TRUE(test_controller_.getProcessor()->setProperty(ListGCSBucket::ListAllVersions.name, "true"));
  const auto& result = test_controller_.trigger();
  EXPECT_EQ(3, result.at(ListGCSBucket::Success).size());
}
