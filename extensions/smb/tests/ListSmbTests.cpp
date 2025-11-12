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
#include "unit/TestUtils.h"
#include "utils/MockSmbConnectionControllerService.h"
#include "unit/SingleProcessorTestController.h"
#include "range/v3/algorithm/count_if.hpp"
#include "range/v3/algorithm/find_if.hpp"
#include "core/Resource.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::extensions::smb::test {

REGISTER_RESOURCE(MockSmbConnectionControllerService, ControllerService);

TEST_CASE("ListSmb invalid network path") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<ListSmb>("ListSmb")};
  const auto list_smb = controller.getProcessor();
  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  REQUIRE(controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Hostname, utils::OsUtils::getHostName().value_or("localhost")));
  REQUIRE(controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Share, "some_share_that_does_not_exists"));
  REQUIRE(controller.plan->setProperty(list_smb, ListSmb::ConnectionControllerService, "smb_connection_controller_service"));
  const auto trigger_results = controller.trigger();
  CHECK(trigger_results.at(ListSmb::Success).empty());
  CHECK(list_smb->isYield());
}

bool checkForFlowFileWithAttributes(const std::vector<std::shared_ptr<core::FlowFile>>& result, ListSmbExpectedAttributes expected_attributes) {
  auto matching_flow_file = ranges::find_if(result, [&](const auto& flow_file) { return flow_file->getAttribute(core::SpecialFlowAttribute::FILENAME) == expected_attributes.expected_filename; });
  if (matching_flow_file == result.end()) {
    return false;
  }
  expected_attributes.checkAttributes(**matching_flow_file);
  return true;
}

TEST_CASE("ListSmb tests") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<ListSmb>("ListSmb")};
  const auto list_smb = controller.getProcessor();

  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  auto mock_smb_connection_controller_service = smb_connection_node->getControllerServiceImplementation<MockSmbConnectionControllerService>();
  REQUIRE(mock_smb_connection_controller_service);
  mock_smb_connection_controller_service->setPath(controller.createTempDirectory());

  auto a_expected_attributes = mock_smb_connection_controller_service->addFile("a.foo", std::string(10_KiB, 'a'), 5min);
  auto b_expected_attributes = mock_smb_connection_controller_service->addFile("b.foo", std::string(13_KiB, 'b'), 1h);
  auto c_expected_attributes = mock_smb_connection_controller_service->addFile("c.bar", std::string(1_KiB, 'c'), 2h);
  auto d_expected_attributes = mock_smb_connection_controller_service->addFile(std::filesystem::path("subdir") / "some" / "d.foo", std::string(100, 'd'), 10min);
  auto e_expected_attributes = mock_smb_connection_controller_service->addFile(std::filesystem::path("subdir2") /"e.foo", std::string(1, 'e'), 10s);
  auto f_expected_attributes = mock_smb_connection_controller_service->addFile(std::filesystem::path("third") / "f.bar", std::string(50_KiB, 'f'), 30min);
  auto g_expected_attributes = mock_smb_connection_controller_service->addFile("g.foo", std::string(50_KiB, 'f'), 30min);
  auto hide_file_error = minifi::test::utils::hide_file(mock_smb_connection_controller_service->getPath() / "g.foo");
  REQUIRE_FALSE(hide_file_error);

  REQUIRE((a_expected_attributes && b_expected_attributes && c_expected_attributes && d_expected_attributes && e_expected_attributes && f_expected_attributes && g_expected_attributes));

  REQUIRE(controller.plan->setProperty(list_smb, ListSmb::ConnectionControllerService, "smb_connection_controller_service"));

  SECTION("FileFilter without subdirs") {
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::FileFilter, ".*\\.foo"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::RecurseSubdirectories, "false"));
    const auto trigger_results = controller.trigger();
    CHECK(trigger_results.at(ListSmb::Success).size() == 2);
    CHECK_FALSE(list_smb->isYield());
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *a_expected_attributes));
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *b_expected_attributes));
  }

  SECTION("Input directory") {
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::InputDirectory, "subdir"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::RecurseSubdirectories, "true"));
    const auto trigger_results = controller.trigger();
    CHECK(trigger_results.at(ListSmb::Success).size() == 1);
    CHECK_FALSE(list_smb->isYield());
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *d_expected_attributes));
  }

  SECTION("PathFilter and FileFilter") {
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::FileFilter, ".*\\.foo"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::RecurseSubdirectories, "true"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::PathFilter, "subdir.*"));
    const auto trigger_results = controller.trigger();
    CHECK(trigger_results.at(ListSmb::Success).size() == 2);
    CHECK_FALSE(list_smb->isYield());
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *d_expected_attributes));
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *e_expected_attributes));
  }

  SECTION("Subdirs with age restriction") {
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::MinimumFileAge, "3min"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::MaximumFileAge, "59min"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::RecurseSubdirectories, "true"));
    const auto trigger_results = controller.trigger();
    CHECK(trigger_results.at(ListSmb::Success).size() == 3);
    CHECK_FALSE(list_smb->isYield());
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *a_expected_attributes));
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *d_expected_attributes));
  }

  SECTION("Subdirs with size restriction") {
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::MinimumFileSize, "2 KB"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::MaximumFileSize, "20 KB"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::RecurseSubdirectories, "true"));
    const auto trigger_results = controller.trigger();
    CHECK(trigger_results.at(ListSmb::Success).size() == 2);
    CHECK_FALSE(list_smb->isYield());
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *a_expected_attributes));
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *b_expected_attributes));
  }

  SECTION("Dont ignore hidden files") {
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::IgnoreHiddenFiles, "false"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::FileFilter, ".*\\.foo"));
    REQUIRE(controller.plan->setProperty(list_smb, ListSmb::RecurseSubdirectories, "false"));
    const auto trigger_results = controller.trigger();
    CHECK(trigger_results.at(ListSmb::Success).size() == 3);
    CHECK_FALSE(list_smb->isYield());
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *a_expected_attributes));
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *b_expected_attributes));
    CHECK(checkForFlowFileWithAttributes(trigger_results.at(ListSmb::Success), *g_expected_attributes));
  }

  const auto second_trigger = controller.trigger();
  CHECK(second_trigger.at(ListSmb::Success).empty());
  CHECK(list_smb->isYield());
}

}  // namespace org::apache::nifi::minifi::extensions::smb::test
