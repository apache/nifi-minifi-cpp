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
#include "FetchSmb.h"
#include "SmbConnectionControllerService.h"
#include "utils/MockSmbConnectionControllerService.h"
#include "unit/SingleProcessorTestController.h"
#include "utils/OsUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::extensions::smb::test {

REGISTER_RESOURCE(MockSmbConnectionControllerService, ControllerService);

TEST_CASE("FetchSmb invalid network path") {
  const auto fetch_smb = std::make_shared<FetchSmb>("FetchSmb");
  minifi::test::SingleProcessorTestController controller{fetch_smb};
  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  REQUIRE(controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Hostname, utils::OsUtils::getHostName().value_or("localhost")));
  REQUIRE(controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Share, "some_share_that_does_not_exist"));
  REQUIRE(controller.plan->setProperty(fetch_smb, FetchSmb::ConnectionControllerService, "smb_connection_controller_service"));
  const auto trigger_results = controller.trigger("", {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}, {std::string(core::SpecialFlowAttribute::PATH), ""}});
  CHECK(trigger_results.at(FetchSmb::Success).empty());
  CHECK(trigger_results.at(FetchSmb::Failure).empty());
  CHECK(fetch_smb->isYield());
}

TEST_CASE("FetchSmb tests") {
  const auto fetch_smb = std::make_shared<FetchSmb>("FetchSmb");
  minifi::test::SingleProcessorTestController controller{fetch_smb};

  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  auto mock_smb_connection_controller_service = std::dynamic_pointer_cast<MockSmbConnectionControllerService>(smb_connection_node->getControllerServiceImplementation());
  REQUIRE(mock_smb_connection_controller_service);

  constexpr std::string_view a_content = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus malesuada elit odio, sit amet viverra ante venenatis eget.";
  constexpr std::string_view b_content = "Phasellus sed pharetra velit. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus.";
  constexpr std::string_view original_content = "Morbi blandit tincidunt sem ac interdum. Aenean at mauris non augue rhoncus finibus quis vel augue.";

  mock_smb_connection_controller_service->setPath(controller.createTempDirectory());

  auto a_expected_attributes = mock_smb_connection_controller_service->addFile("a.foo", a_content, 5min);
  auto b_expected_attributes = mock_smb_connection_controller_service->addFile("subdir/b.foo", b_content, 5min);

  REQUIRE(controller.plan->setProperty(fetch_smb, FetchSmb::ConnectionControllerService, "smb_connection_controller_service"));

  SECTION("Without Remote File property") {
  }
  SECTION("Remote File Property with expression language (unix separator)") {
    REQUIRE(controller.plan->setProperty(fetch_smb, FetchSmb::RemoteFile, "${path}/${filename}"));
  }
  SECTION("Remote File Property with expression language (windows separator)") {
    REQUIRE(controller.plan->setProperty(fetch_smb, FetchSmb::RemoteFile, "${path}\\${filename}"));
  }

  {
    const auto trigger_results = controller.trigger(original_content, {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}, {std::string(core::SpecialFlowAttribute::PATH), ""}});

    CHECK(trigger_results.at(FetchSmb::Failure).empty());
    REQUIRE(trigger_results.at(FetchSmb::Success).size() == 1);
    auto succeeded_flow_file = trigger_results.at(FetchSmb::Success)[0];

    CHECK(controller.plan->getContent(succeeded_flow_file) == a_content);
    CHECK_FALSE(succeeded_flow_file->getAttribute(FetchSmb::ErrorCode.name));
    CHECK_FALSE(succeeded_flow_file->getAttribute(FetchSmb::ErrorMessage.name));
  }
  {
    const auto trigger_results = controller.trigger(original_content, {{std::string(core::SpecialFlowAttribute::FILENAME), "b.foo"}, {std::string(core::SpecialFlowAttribute::PATH), "subdir"}});

    CHECK(trigger_results.at(FetchSmb::Failure).empty());
    REQUIRE(trigger_results.at(FetchSmb::Success).size() == 1);
    auto succeeded_flow_file = trigger_results.at(FetchSmb::Success)[0];

    CHECK(controller.plan->getContent(succeeded_flow_file) == b_content);
    CHECK_FALSE(succeeded_flow_file->getAttribute(FetchSmb::ErrorCode.name));
    CHECK_FALSE(succeeded_flow_file->getAttribute(FetchSmb::ErrorMessage.name));
  }
  {
    const auto trigger_results = controller.trigger(original_content, {{std::string(core::SpecialFlowAttribute::FILENAME), "c.foo"}, {std::string(core::SpecialFlowAttribute::PATH), "subdir"}});

    CHECK(trigger_results.at(FetchSmb::Success).empty());
    REQUIRE(trigger_results.at(FetchSmb::Failure).size() == 1);
    auto failed_flow_file = trigger_results.at(FetchSmb::Failure)[0];

    CHECK(controller.plan->getContent(failed_flow_file) == original_content);
    CHECK(failed_flow_file->getAttribute(FetchSmb::ErrorCode.name) == "2");
    CHECK(failed_flow_file->getAttribute(FetchSmb::ErrorMessage.name) == "Error opening file: No such file or directory");
  }
}

}  // namespace org::apache::nifi::minifi::extensions::smb::test
