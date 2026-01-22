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
#define EXTENSION_LIST "minifi-gcp"  // NOLINT(cppcoreguidelines-macro-usage)

#include "unit/TestBase.h"
#include "gtest/gtest.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "core/Resource.h"
#include "core/Processor.h"
#include "minifi-cpp/core/controller/ControllerServiceNode.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "unit/DummyProcessor.h"
#include "utils/Environment.h"

namespace minifi_gcp = org::apache::nifi::minifi::extensions::gcp;
using GCPCredentialsControllerService = minifi_gcp::GCPCredentialsControllerService;

namespace {

std::string create_mock_service_json() {
  rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
  root.AddMember("type", "service_account", root.GetAllocator());
  root.AddMember("project_id", "mock_project_id", root.GetAllocator());
  root.AddMember("private_key_id", "my_private_key_id", root.GetAllocator());
  root.AddMember("private_key", "-----BEGIN PRIVATE KEY-----\n"
                                "MIIBVAIBADANBgkqhkiG9w0BAQEFAASCAT4wggE6AgEAAkEAo2Eyw6KfcYOSD0D1\n"
                                "7cw3+M/Qkv5xXwaxxHlAZk+Bscjkm2S37iQwm87mLnhyr7nnAUXZTHsR6SDrBhj7\n"
                                "9xvM1QIDAQABAkB1RTJL7HGn5/myCz27J4fRh1E+AXbc75Av55yLE2yTb+qwfX3m\n"
                                "eAw0dZAIRQ8ZuXw7su71bW2YyB43RwXOnGWtAiEA0zo0bu6h8LPAK9y65zw8KNuF\n"
                                "A+Rif5+7K12uv1XgCWsCIQDGAqSH6JToI7yHOup47XM1CKMnjBDe67ExJPuDH3HS\n"
                                "vwIgAI+RABJmH6t6gSNO47pHNpyOl9oNYOVdq9nN0vg5Zg0CIQDEDjXOg9F8kHXJ\n"
                                "B+LFXYamyiiRrbO+pWvKly2ZRPc0jQIgfZyH0JGjJKZTLog14owyAA+JUkHTh7Em\n"
                                "8o9ev8MeLoM=\n"
                                "-----END PRIVATE KEY-----", root.GetAllocator());
  root.AddMember("client_email", "my_client_email", root.GetAllocator());
  root.AddMember("client_id", "my_client_id", root.GetAllocator());
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  root.Accept(writer);
  return buffer.GetString();
}

std::optional<std::filesystem::path> create_mock_json_file(const std::filesystem::path& dir_path) {
  std::filesystem::path path = dir_path / "mock_credentials.json";
  std::ofstream p{path};
  if (!p)
    return std::nullopt;
  p << create_mock_service_json();
  p.close();
  return path;
}
}  // namespace

class GCPCredentialsTests : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(gcp_credentials_node_);
    ASSERT_TRUE(gcp_credentials_);
    plan_->addProcessor("DummyProcessor", "dummy_processor");
  }
  TestController test_controller_;
  std::shared_ptr<TestPlan> plan_ = test_controller_.createPlan();
  std::shared_ptr<minifi::core::controller::ControllerServiceNode> gcp_credentials_node_ = plan_->addController("GCPCredentialsControllerService", "gcp_credentials_controller_service");
  std::shared_ptr<GCPCredentialsControllerService> gcp_credentials_ = std::dynamic_pointer_cast<GCPCredentialsControllerService>(gcp_credentials_node_->getControllerServiceImplementation());
};

TEST_F(GCPCredentialsTests, DefaultGCPCredentialsWithEnv) {
  auto temp_directory = test_controller_.createTempDirectory();
  auto path = create_mock_json_file(temp_directory);
  ASSERT_TRUE(path.has_value());
  minifi::utils::Environment::setEnvironmentVariable("GOOGLE_APPLICATION_CREDENTIALS", path->string().c_str());
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_DEFAULT_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonWithoutProperty) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_JSON_FILE));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_EQ(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonWithProperty) {
  auto temp_directory = test_controller_.createTempDirectory();
  auto path = create_mock_json_file(temp_directory);
  ASSERT_TRUE(path.has_value());
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_JSON_FILE));
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::JsonFilePath, path->string());
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromComputeEngineVM) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, AnonymousCredentials) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonContentsWithoutProperty) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_JSON_CONTENTS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_EQ(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonContentsWithProperty) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, magic_enum::enum_name(minifi_gcp::CredentialsLocation::USE_JSON_CONTENTS));
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::JsonContents, create_mock_service_json());
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}
