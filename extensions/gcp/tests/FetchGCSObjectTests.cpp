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
#include "../processors/FetchGCSObject.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "GCPAttributes.h"
#include "core/Resource.h"
#include "SingleProcessorTestController.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/testing/canonical_errors.h"

namespace gcs = ::google::cloud::storage;
namespace minifi_gcp = org::apache::nifi::minifi::extensions::gcp;

using FetchGCSObject = org::apache::nifi::minifi::extensions::gcp::FetchGCSObject;
using GCPCredentialsControllerService = org::apache::nifi::minifi::extensions::gcp::GCPCredentialsControllerService;

namespace {
class FetchGCSObjectMocked : public FetchGCSObject {
  using org::apache::nifi::minifi::extensions::gcp::FetchGCSObject::FetchGCSObject;
 public:
  static constexpr const char* Description = "FetchGCSObjectMocked";

  gcs::Client getClient() const override {
    return gcs::testing::UndecoratedClientFromMock(mock_client_);
  }
  std::shared_ptr<gcs::testing::MockClient> mock_client_ = std::make_shared<gcs::testing::MockClient>();
};
REGISTER_RESOURCE(FetchGCSObjectMocked, Processor);
}  // namespace

class FetchGCSObjectTests : public ::testing::Test {
 public:
  void SetUp() override {
    gcp_credentials_node_ = test_controller_.plan->addController("GCPCredentialsControllerService", "gcp_credentials_controller_service");
    test_controller_.plan->setProperty(gcp_credentials_node_,
                                       GCPCredentialsControllerService::CredentialsLoc,
                                       magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS));
    test_controller_.plan->setProperty(fetch_gcs_object_,
                                       FetchGCSObject::GCPCredentials,
                                       "gcp_credentials_controller_service");
  }
  std::shared_ptr<FetchGCSObjectMocked> fetch_gcs_object_ = std::make_shared<FetchGCSObjectMocked>("FetchGCSObjectMocked");
  org::apache::nifi::minifi::test::SingleProcessorTestController test_controller_{fetch_gcs_object_};
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  gcp_credentials_node_;
};

TEST_F(FetchGCSObjectTests, MissingBucket) {
  EXPECT_CALL(*fetch_gcs_object_->mock_client_, CreateResumableUpload).Times(0);
  EXPECT_TRUE(test_controller_.plan->setProperty(fetch_gcs_object_, FetchGCSObject::Bucket, ""));
  const auto& result = test_controller_.trigger("hello world");
  EXPECT_EQ(0, result.at(FetchGCSObject::Success).size());
  ASSERT_EQ(1, result.at(FetchGCSObject::Failure).size());
  EXPECT_EQ(std::nullopt, result.at(FetchGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_DOMAIN));
  EXPECT_EQ(std::nullopt, result.at(FetchGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_REASON));
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(FetchGCSObject::Failure)[0]));
}

TEST_F(FetchGCSObjectTests, ServerError) {
  EXPECT_CALL(*fetch_gcs_object_->mock_client_, ReadObject)
      .WillOnce([](gcs::internal::ReadObjectRangeRequest const& request) {
        EXPECT_EQ(request.bucket_name(), "bucket-from-property") << request;
        auto mock_source = std::make_unique<gcs::testing::MockObjectReadSource>();
        ::testing::InSequence seq;
        EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(testing::Return(true));
        EXPECT_CALL(*mock_source, Read)
            .WillOnce(testing::Return(google::cloud::Status(
                google::cloud::StatusCode::kInvalidArgument,
                "Invalid Argument")));
        EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(testing::Return(false));

        std::unique_ptr<gcs::internal::ObjectReadSource> object_read_source = std::move(mock_source);
        return google::cloud::make_status_or(std::move(object_read_source));
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(fetch_gcs_object_, FetchGCSObject::Bucket, "bucket-from-property"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}});
  EXPECT_EQ(0, result.at(FetchGCSObject::Success).size());
  ASSERT_EQ(1, result.at(FetchGCSObject::Failure).size());
  EXPECT_NE(std::nullopt, result.at(FetchGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_DOMAIN));
  EXPECT_NE(std::nullopt, result.at(FetchGCSObject::Failure)[0]->getAttribute(minifi_gcp::GCS_ERROR_REASON));
}

TEST_F(FetchGCSObjectTests, HappyPath) {
  std::string const text = "stored text";
  std::size_t offset = 0;
  // Simulate a Read() call in the MockObjectReadSource object created below
  auto simulate_read = [&text, &offset](void* buf, std::size_t n) {
    auto const l = (std::min)(n, text.size() - offset);
    std::memcpy(buf, text.data() + offset, l);
    offset += l;
    return gcs::internal::ReadSourceResult{
        l, gcs::internal::HttpResponse{200, {}, {}}};
  };
  EXPECT_CALL(*fetch_gcs_object_->mock_client_, ReadObject)
      .WillOnce([&](gcs::internal::ReadObjectRangeRequest const& request) {
        EXPECT_EQ(request.bucket_name(), "bucket-from-attribute") << request;
        EXPECT_TRUE(request.HasOption<gcs::Generation>());
        EXPECT_TRUE(request.GetOption<gcs::Generation>().has_value());
        EXPECT_EQ(23, request.GetOption<gcs::Generation>().value());
        std::unique_ptr<gcs::testing::MockObjectReadSource> mock_source(new gcs::testing::MockObjectReadSource);
        ::testing::InSequence seq;
        EXPECT_CALL(*mock_source, IsOpen()).WillRepeatedly(testing::Return(true));
        EXPECT_CALL(*mock_source, Read).WillOnce(simulate_read);
        EXPECT_CALL(*mock_source, IsOpen()).WillRepeatedly(testing::Return(false));

        return google::cloud::make_status_or(
            std::unique_ptr<gcs::internal::ObjectReadSource>(
                std::move(mock_source)));
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(fetch_gcs_object_, FetchGCSObject::ObjectGeneration, "${gcs.generation}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}, {std::string(minifi_gcp::GCS_GENERATION), "23"}});
  ASSERT_EQ(1, result.at(FetchGCSObject::Success).size());
  EXPECT_EQ(0, result.at(FetchGCSObject::Failure).size());
  EXPECT_EQ("stored text", test_controller_.plan->getContent(result.at(FetchGCSObject::Success)[0]));
}

TEST_F(FetchGCSObjectTests, EmptyGeneration) {
  std::string const text = "stored text";
  std::size_t offset = 0;
  // Simulate a Read() call in the MockObjectReadSource object created below
  auto simulate_read = [&text, &offset](void* buf, std::size_t n) {
    auto const l = (std::min)(n, text.size() - offset);
    std::memcpy(buf, text.data() + offset, l);
    offset += l;
    return gcs::internal::ReadSourceResult{
        l, gcs::internal::HttpResponse{200, {}, {}}};
  };
  EXPECT_CALL(*fetch_gcs_object_->mock_client_, ReadObject)
      .WillOnce([&](gcs::internal::ReadObjectRangeRequest const& request) {
        EXPECT_EQ(request.bucket_name(), "bucket-from-attribute") << request;
        EXPECT_FALSE(request.HasOption<gcs::Generation>());
        std::unique_ptr<gcs::testing::MockObjectReadSource> mock_source(new gcs::testing::MockObjectReadSource);
        ::testing::InSequence seq;
        EXPECT_CALL(*mock_source, IsOpen()).WillRepeatedly(testing::Return(true));
        EXPECT_CALL(*mock_source, Read).WillOnce(simulate_read);
        EXPECT_CALL(*mock_source, IsOpen()).WillRepeatedly(testing::Return(false));

        return google::cloud::make_status_or(
            std::unique_ptr<gcs::internal::ObjectReadSource>(
                std::move(mock_source)));
      });
  EXPECT_TRUE(test_controller_.plan->setProperty(fetch_gcs_object_, FetchGCSObject::ObjectGeneration, "${gcs.generation}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}});
  ASSERT_EQ(1, result.at(FetchGCSObject::Success).size());
  EXPECT_EQ(0, result.at(FetchGCSObject::Failure).size());
  EXPECT_EQ("stored text", test_controller_.plan->getContent(result.at(FetchGCSObject::Success)[0]));
}

TEST_F(FetchGCSObjectTests, InvalidGeneration) {
  EXPECT_TRUE(test_controller_.plan->setProperty(fetch_gcs_object_, FetchGCSObject::ObjectGeneration, "${gcs.generation}"));
  const auto& result = test_controller_.trigger("hello world", {{std::string(minifi_gcp::GCS_BUCKET_ATTR), "bucket-from-attribute"}, {std::string(minifi_gcp::GCS_GENERATION), "23 banana"}});
  ASSERT_EQ(0, result.at(FetchGCSObject::Success).size());
  EXPECT_EQ(1, result.at(FetchGCSObject::Failure).size());
  EXPECT_EQ("hello world", test_controller_.plan->getContent(result.at(FetchGCSObject::Failure)[0]));
}
