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
#include <memory>
#include <string>
#include <fstream>
#include "FlowController.h"
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "properties/Configure.h"
#include "GetFile.h"
#include "core/Core.h"
#include "Exception.h"
#include "core/FlowFile.h"
#include "unit/MockClasses.h"
#include "unit/ProvenanceTestHelper.h"
#include "core/Processor.h"
#include "core/controller/ControllerServiceNodeMap.h"
#include "core/controller/StandardControllerServiceNode.h"

namespace ControllerServiceTests {

TEST_CASE("Test ControllerServicesMap", "[cs1]") {
  core::controller::ControllerServiceNodeMap map;
  REQUIRE(map.getAllControllerServices().empty());

  std::shared_ptr<core::controller::ControllerService> service = std::make_shared<MockControllerService>();
  auto testNode = std::make_shared<core::controller::StandardControllerServiceNode>(service, "ID", std::make_shared<minifi::ConfigureImpl>());

  map.put("ID", testNode);
  REQUIRE(1 == map.getAllControllerServices().size());

  REQUIRE(nullptr != map.get("ID"));

  REQUIRE(false== map.put("", testNode));
  REQUIRE(false== map.put("", nullptr));

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
  std::shared_ptr<core::controller::ControllerService> service = std::make_shared<MockControllerService>();
  auto testNode = std::make_shared<core::controller::StandardControllerServiceNode>(service, id, std::make_shared<minifi::ConfigureImpl>());

  return testNode;
}

} /**  namespace ControllerServiceTests **/
