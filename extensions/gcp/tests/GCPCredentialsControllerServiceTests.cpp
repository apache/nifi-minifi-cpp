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
#define EXTENSION_LIST "minifi-gcp"

#include "TestBase.h"
#include "gtest/gtest.h"
#include "../controllerservices/GCPCredentialsControllerService.h"
#include "core/Resource.h"
#include "core/Processor.h"
#include "core/controller/ControllerServiceNode.h"
#include "rapidjson/document.h"
#include "rapidjson/stream.h"
#include "rapidjson/writer.h"
#include "DummyProcessor.h"
#include "utils/Environment.h"

namespace minifi_gcp = org::apache::nifi::minifi::extensions::gcp;
using GCPCredentialsControllerService = minifi_gcp::GCPCredentialsControllerService;

namespace {

std::string create_mock_service_json() {
  rapidjson::Document root = rapidjson::Document(rapidjson::kObjectType);
  root.AddMember("type", "service_account", root.GetAllocator());
  root.AddMember("project_id", "mock_project_id", root.GetAllocator());
  root.AddMember("private_key_id", "my_private_key_id", root.GetAllocator());
  root.AddMember("private_key", "-----BEGIN RSA PRIVATE KEY-----\n"
                                "MIIBOgIBAAJBAKNhMsOin3GDkg9A9e3MN/jP0JL+cV8GscR5QGZPgbHI5Jtkt+4k\n"
                                "MJvO5i54cq+55wFF2Ux7Eekg6wYY+/cbzNUCAwEAAQJAdUUyS+xxp+f5sgs9uyeH\n"
                                "0YdRPgF23O+QL+ecixNsk2/qsH195ngMNHWQCEUPGbl8O7Lu9W1tmMgeN0cFzpxl\n"
                                "rQIhANM6NG7uofCzwCvcuuc8PCjbhQPkYn+fuytdrr9V4AlrAiEAxgKkh+iU6CO8\n"
                                "hzrqeO1zNQijJ4wQ3uuxMST7gx9x0r8CIACPkQASZh+reoEjTuO6RzacjpfaDWDl\n"
                                "XavZzdL4OWYNAiEAxA41zoPRfJB1yQfixV2Gpsooka2zvqVrypctmUT3NI0CIH2c\n"
                                "h9CRoySmUy6INeKMMgAPiVJB04exJvKPXr/DHi6D\n"
                                "-----END RSA PRIVATE KEY-----", root.GetAllocator());
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
  TestController test_controller_{};
  std::shared_ptr<TestPlan> plan_ = test_controller_.createPlan();
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  gcp_credentials_node_ = plan_->addController("GCPCredentialsControllerService", "gcp_credentials_controller_service");
  std::shared_ptr<GCPCredentialsControllerService> gcp_credentials_ = std::dynamic_pointer_cast<GCPCredentialsControllerService>(gcp_credentials_node_->getControllerServiceImplementation());
};

TEST_F(GCPCredentialsTests, DefaultGCPCredentialsWithoutEnv) {
  minifi::utils::Environment::unsetEnvironmentVariable("GOOGLE_APPLICATION_CREDENTIALS");
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_DEFAULT_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_EQ(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, DefaultGCPCredentialsWithEnv) {
  auto temp_directory = test_controller_.createTempDirectory();
  auto path = create_mock_json_file(temp_directory);
  ASSERT_TRUE(path.has_value());
  minifi::utils::Environment::setEnvironmentVariable("GOOGLE_APPLICATION_CREDENTIALS", path->string().c_str());
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_DEFAULT_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonWithoutProperty) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_JSON_FILE));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_EQ(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonWithProperty) {
  auto temp_directory = test_controller_.createTempDirectory();
  auto path = create_mock_json_file(temp_directory);
  ASSERT_TRUE(path.has_value());
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_JSON_FILE));
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::JsonFilePath, path->string());
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromComputeEngineVM) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_COMPUTE_ENGINE_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, AnonymousCredentials) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_ANONYMOUS_CREDENTIALS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonContentsWithoutProperty) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_JSON_CONTENTS));
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_EQ(nullptr, gcp_credentials_->getCredentials());
}

TEST_F(GCPCredentialsTests, CredentialsFromJsonContentsWithProperty) {
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::CredentialsLoc, toString(minifi_gcp::CredentialsLocation::USE_JSON_CONTENTS));
  plan_->setProperty(gcp_credentials_node_, GCPCredentialsControllerService::JsonContents, create_mock_service_json());
  ASSERT_NO_THROW(test_controller_.runSession(plan_));
  EXPECT_NE(nullptr, gcp_credentials_->getCredentials());
}
