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
#include "ListSmb.h"
#include "FetchSmb.h"
#include "utils/MockSmbConnectionControllerService.h"
#include "unit/ReadFromFlowFileTestProcessor.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::smb::test {

REGISTER_RESOURCE(MockSmbConnectionControllerService, ControllerService);

using minifi::processors::ReadFromFlowFileTestProcessor;

TEST_CASE("ListSmb and FetchSmb work together") {
  TestController controller;
  auto plan = controller.createPlan();
  auto list_smb = plan->addProcessor<ListSmb>("list_smb");
  auto fetch_smb = plan->addProcessor<ListSmb>("fetch_smb");
  auto read_from_success_relationship = plan->addProcessor<ReadFromFlowFileTestProcessor>("read_from_success_relationship");
  auto read_from_failure_relationship = plan->addProcessor<ReadFromFlowFileTestProcessor>("read_from_failure_relationship");

  plan->addConnection(list_smb, ListSmb::Success, fetch_smb);

  plan->addConnection(fetch_smb, FetchSmb::Success, read_from_success_relationship);
  plan->addConnection(fetch_smb, FetchSmb::Failure, read_from_failure_relationship);

  auto smb_connection_node = plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  auto mock_smb_connection_controller_service = smb_connection_node->getControllerServiceImplementation<MockSmbConnectionControllerService>();
  REQUIRE(mock_smb_connection_controller_service);

  plan->setProperty(list_smb, ListSmb::ConnectionControllerService, "smb_connection_controller_service");
  plan->setProperty(fetch_smb, FetchSmb::ConnectionControllerService, "smb_connection_controller_service");

  read_from_success_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});
  read_from_failure_relationship->setAutoTerminatedRelationships(std::array<core::Relationship, 1>{ReadFromFlowFileTestProcessor::Success});

  mock_smb_connection_controller_service->setPath(controller.createTempDirectory());
  constexpr std::string_view content = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus malesuada elit odio, sit amet viverra ante venenatis eget.";
  REQUIRE(mock_smb_connection_controller_service->addFile("input_dir/sub_dir/b.foo", content, 5min));

  SECTION("With Input Directory") {
    plan->setProperty(list_smb, ListSmb::InputDirectory, "input_dir");
  }

  SECTION("Without Input Directory") {
  }

  controller.runSession(plan);
  CHECK(read_from_success_relationship.get().numberOfFlowFilesRead() == 1);
  CHECK(read_from_failure_relationship.get().numberOfFlowFilesRead() == 0);
}

}  // namespace org::apache::nifi::minifi::extensions::smb::test
