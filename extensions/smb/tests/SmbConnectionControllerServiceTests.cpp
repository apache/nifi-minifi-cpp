// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
