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
#include "PutSmb.h"
#include "utils/MockSmbConnectionControllerService.h"
#include "unit/SingleProcessorTestController.h"
#include "range/v3/algorithm/count_if.hpp"
#include "core/Resource.h"
#include "unit/ProcessorUtils.h"

namespace org::apache::nifi::minifi::extensions::smb::test {

REGISTER_RESOURCE(MockSmbConnectionControllerService, ControllerService);

std::string checkFileContent(const std::filesystem::path& path) {
  gsl_Expects(std::filesystem::exists(path));
  std::ifstream if_stream(path);
  return {std::istreambuf_iterator<char>(if_stream), std::istreambuf_iterator<char>()};
}

TEST_CASE("PutSmb invalid network path") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<PutSmb>("PutSmb")};
  const auto put_smb = controller.getProcessor();
  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  REQUIRE(controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Hostname, utils::OsUtils::getHostName().value_or("localhost")));
  REQUIRE(controller.plan->setProperty(smb_connection_node, SmbConnectionControllerService::Share, "some_share_that_does_not_exists"));
  REQUIRE(controller.plan->setProperty(put_smb, PutSmb::ConnectionControllerService, "smb_connection_controller_service"));
  const auto trigger_results = controller.trigger("", {{std::string(core::SpecialFlowAttribute::FILENAME), "a.foo"}, {std::string(core::SpecialFlowAttribute::PATH), ""}});
  CHECK(trigger_results.at(PutSmb::Success).empty());
  CHECK(trigger_results.at(PutSmb::Failure).empty());
  CHECK(put_smb->isYield());
}

TEST_CASE("PutSmb conflict resolution test") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<PutSmb>("PutSmb")};
  const auto put_smb = controller.getProcessor();

  auto temp_directory = controller.createTempDirectory();
  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  auto mock_smb_connection_controller_service = smb_connection_node->getControllerServiceImplementation<MockSmbConnectionControllerService>();
  REQUIRE(mock_smb_connection_controller_service);
  mock_smb_connection_controller_service->setPath(temp_directory);

  controller.plan->setProperty(put_smb, PutSmb::ConnectionControllerService, "smb_connection_controller_service");

  SECTION("Replace") {
    controller.plan->setProperty(put_smb, PutSmb::ConflictResolution, magic_enum::enum_name(PutSmb::FileExistsResolutionStrategy::replace));

    std::string file_name = "my_file.txt";

    CHECK_FALSE(std::filesystem::exists(temp_directory / file_name));

    const auto first_trigger_results = controller.trigger("alpha", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");

    const auto second_trigger_results = controller.trigger("beta", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(second_trigger_results.at(PutSmb::Failure).empty());
    CHECK(second_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "beta");
  }

  SECTION("Ignore") {
    controller.plan->setProperty(put_smb, PutSmb::ConflictResolution, magic_enum::enum_name(PutSmb::FileExistsResolutionStrategy::ignore));

    std::string file_name = "my_file.txt";

    CHECK_FALSE(std::filesystem::exists(temp_directory / file_name));

    const auto first_trigger_results = controller.trigger("alpha", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");

    const auto second_trigger_results = controller.trigger("beta", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(second_trigger_results.at(PutSmb::Failure).empty());
    CHECK(second_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");
  }

  SECTION("Fail") {
    controller.plan->setProperty(put_smb, PutSmb::ConflictResolution, magic_enum::enum_name(PutSmb::FileExistsResolutionStrategy::fail));

    std::string file_name = "my_file.txt";

    CHECK_FALSE(std::filesystem::exists(temp_directory / file_name));

    const auto first_trigger_results = controller.trigger("alpha", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");

    const auto second_trigger_results = controller.trigger("beta", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(second_trigger_results.at(PutSmb::Failure).size() == 1);
    CHECK(second_trigger_results.at(PutSmb::Success).empty());

    CHECK(std::filesystem::exists(temp_directory / file_name));
    CHECK(checkFileContent(temp_directory / file_name) == "alpha");
  }
}

TEST_CASE("PutSmb create missing dirs test") {
  minifi::test::SingleProcessorTestController controller{minifi::test::utils::make_processor<PutSmb>("PutSmb")};
  const auto put_smb = controller.getProcessor();

  auto temp_directory = controller.createTempDirectory();
  auto smb_connection_node = controller.plan->addController("MockSmbConnectionControllerService", "smb_connection_controller_service");
  auto mock_smb_connection_controller_service = smb_connection_node->getControllerServiceImplementation<MockSmbConnectionControllerService>();
  REQUIRE(mock_smb_connection_controller_service);
  mock_smb_connection_controller_service->setPath(temp_directory);

  controller.plan->setProperty(put_smb, PutSmb::ConnectionControllerService, "smb_connection_controller_service");
  controller.plan->setProperty(put_smb, PutSmb::Directory, "a/b");

  SECTION("Create missing dirs") {
    controller.plan->setProperty(put_smb, PutSmb::CreateMissingDirectories, "true");
    std::string file_name = "my_file.txt";

    auto expected_path = temp_directory / "a" / "b" / file_name;

    CHECK_FALSE(std::filesystem::exists(expected_path));

    const auto first_trigger_results = controller.trigger("alpha", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).empty());
    CHECK(first_trigger_results.at(PutSmb::Success).size() == 1);

    REQUIRE(std::filesystem::exists(expected_path));
    CHECK(checkFileContent(expected_path) == "alpha");
  }

  SECTION("Don't create missing dirs") {
    controller.plan->setProperty(put_smb, PutSmb::CreateMissingDirectories, "false");
    std::string file_name = "my_file.txt";

    auto expected_path = temp_directory / "a" / "b" / file_name;

    CHECK_FALSE(std::filesystem::exists(expected_path));

    const auto first_trigger_results = controller.trigger("alpha", {{std::string(core::SpecialFlowAttribute::FILENAME), file_name}});

    CHECK(first_trigger_results.at(PutSmb::Failure).size() == 1);
    CHECK(first_trigger_results.at(PutSmb::Success).empty());

    CHECK_FALSE(std::filesystem::exists(expected_path));
  }
}



}  // namespace org::apache::nifi::minifi::extensions::smb::test
