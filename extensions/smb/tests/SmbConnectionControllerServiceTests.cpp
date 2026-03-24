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
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "SmbConnectionControllerService.h"
#include "utils/TempSmbShare.h"
#include "utils/UnicodeConversion.h"

namespace org::apache::nifi::minifi::extensions::smb::test {

struct SmbConnectionControllerServiceFixture {
  SmbConnectionControllerServiceFixture() = default;

  TestController test_controller_{};
  std::shared_ptr<TestPlan> plan_ = test_controller_.createPlan();
  std::shared_ptr<minifi::core::controller::ControllerServiceNode>  smb_connection_node_ = plan_->addController("SmbConnectionControllerService", "smb_connection_controller_service");
  std::shared_ptr<SmbConnectionControllerService> smb_connection_ = smb_connection_node_->getControllerServiceImplementation<SmbConnectionControllerService>();
};


TEST_CASE_METHOD(SmbConnectionControllerServiceFixture, "SmbConnectionControllerService onEnable throws when empty") {
  REQUIRE_THROWS(smb_connection_node_->getControllerServiceImplementation()->onEnable());
}

TEST_CASE_METHOD(SmbConnectionControllerServiceFixture, "SmbConnectionControllerService anonymous connection") {
  auto temp_directory = test_controller_.createTempDirectory();
  auto share_local_name = temp_directory.filename().wstring();

  auto temp_smb_share = TempSmbShare::create(share_local_name, temp_directory.wstring());
  if (!temp_smb_share && temp_smb_share.error() == std::error_code(5, std::system_category())) {
    SKIP("SmbConnectionControllerService tests needs administrator privileges");
  }


  SECTION("Valid share") {
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Hostname, "localhost");
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Share, minifi::utils::to_string(share_local_name));

    REQUIRE_NOTHROW(plan_->finalize());

    auto connection_error = smb_connection_->validateConnection();
    CHECK_FALSE(connection_error);
  }

  SECTION("Invalid share") {
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Hostname, "localhost");
    plan_->setProperty(smb_connection_node_, SmbConnectionControllerService::Share, "invalid_share_name");

    REQUIRE_NOTHROW(plan_->finalize());

    auto connection_error = smb_connection_->validateConnection();
    CHECK(connection_error);
  }
}

}  // namespace org::apache::nifi::minifi::extensions::smb::test
