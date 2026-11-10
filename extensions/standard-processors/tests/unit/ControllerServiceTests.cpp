// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include <memory>
#include <string>
#include <fstream>
#include "FlowController.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "properties/Configure.h"
#include "GetFile.h"
#include "core/Core.h"
#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/FlowFile.h"
#include "unit/MockClasses.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/controller/ControllerServiceNodeMap.h"
#include "core/controller/StandardControllerServiceNode.h"

namespace ControllerServiceTests {

TEST_CASE("Test ControllerServicesMap", "[cs1]") {
  core::controller::ControllerServiceNodeMap map;
  REQUIRE(map.getAllControllerServices().empty());

  auto service = std::make_shared<core::controller::ControllerService>("", utils::Identifier{}, std::make_unique<MockControllerService>());
  auto testNode = std::make_shared<core::controller::StandardControllerServiceNode>(service, "ID", std::make_shared<minifi::ConfigureImpl>());

  map.put("ID", testNode, nullptr);
  REQUIRE(1 == map.getAllControllerServices().size());

  REQUIRE(nullptr != map.get("ID"));

  REQUIRE(false== map.put("", testNode, nullptr));
  REQUIRE(false== map.put("", nullptr, nullptr));

  // ensure the pointer is the same

  REQUIRE(service.get() == map.get("ID")->getControllerServiceImplementation().get());
}

TEST_CASE("Test StandardControllerServiceNode nullPtr", "[cs1]") {
  core::controller::ControllerServiceNodeMap map;

  try {
    auto testNode = std::make_shared<core::controller::StandardControllerServiceNode>(nullptr, "ID", std::make_shared<minifi::ConfigureImpl>());
  } catch (const minifi::Exception &) {
    return;
  }

  FAIL("Should have encountered exception");
}

std::shared_ptr<core::controller::StandardControllerServiceNode> newCsNode(const std::string& id) {
  auto service = std::make_shared<core::controller::ControllerService>("", utils::Identifier{}, std::make_unique<MockControllerService>());
  auto testNode = std::make_shared<core::controller::StandardControllerServiceNode>(service, id, std::make_shared<minifi::ConfigureImpl>());

  return testNode;
}

} /**  namespace ControllerServiceTests **/
